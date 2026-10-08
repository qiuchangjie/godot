#include "../as_host_bridge.h"

#define ANGELSCRIPT_HOST_BRIDGE_TESTS_IMPL
#include "test_angelscript_host_bridge.h"

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "scene/main/node.h"

#include <angelscript.h>

namespace {

int g_invoke_count = 0;
int32_t g_last_method_id = -1;
int32_t g_last_argc = -1;
Variant g_last_arg0;
bool g_last_arg0_was_invalid_object = false;
Variant g_stub_return;
int g_stub_rc = 0;

int _stub_invoke(void *p_user_data, int32_t p_method_id, const Variant *p_args, int32_t p_argc, Variant *r_ret) {
	(void)p_user_data;
	g_invoke_count++;
	g_last_method_id = p_method_id;
	g_last_argc = p_argc;
	g_last_arg0_was_invalid_object = false;
	if (p_argc > 0) {
		g_last_arg0 = p_args[0];
		if (g_last_arg0.get_type() == Variant::OBJECT && g_last_arg0.get_validated_object() == nullptr) {
			g_last_arg0_was_invalid_object = true;
		}
	} else {
		g_last_arg0 = Variant();
	}
	if (r_ret != nullptr) {
		*r_ret = g_stub_return;
	}
	return g_stub_rc;
}

void _reset_stub() {
	g_invoke_count = 0;
	g_last_method_id = -1;
	g_last_argc = -1;
	g_last_arg0 = Variant();
	g_last_arg0_was_invalid_object = false;
	g_stub_return = Variant();
	g_stub_rc = 0;
}

ASHostCallbacks _make_stub() {
	ASHostCallbacks cb;
	cb.abi_version = AS_HOST_BRIDGE_ABI_VERSION;
	cb.struct_size = (uint32_t)sizeof(ASHostCallbacks);
	cb.user_data = nullptr;
	cb.invoke = _stub_invoke;
	return cb;
}

} // namespace

void as_host_bridge_install_validation() {
	ASHostBridge *bridge = ASHostBridge::get_singleton();
	REQUIRE(bridge != nullptr);
	bridge->uninstall();
	CHECK_FALSE(bridge->is_installed());

	CHECK(bridge->install(nullptr) == ERR_INVALID_PARAMETER);

	ASHostCallbacks cb = _make_stub();
	cb.abi_version = AS_HOST_BRIDGE_ABI_VERSION + 1;
	CHECK(bridge->install(&cb) == ERR_INVALID_PARAMETER);
	CHECK_FALSE(bridge->is_installed());

	cb = _make_stub();
	cb.struct_size = 4;
	CHECK(bridge->install(&cb) == ERR_INVALID_PARAMETER);
	CHECK_FALSE(bridge->is_installed());

	cb = _make_stub();
	cb.invoke = nullptr;
	CHECK(bridge->install(&cb) == ERR_INVALID_PARAMETER);
	CHECK_FALSE(bridge->is_installed());

	cb = _make_stub();
	CHECK(bridge->install(&cb) == OK);
	CHECK(bridge->is_installed());

	// 更大的 struct（前向兼容）也应接受。
	cb.struct_size = (uint32_t)sizeof(ASHostCallbacks) + 8;
	CHECK(bridge->install(&cb) == OK);

	bridge->uninstall();
	CHECK_FALSE(bridge->is_installed());
}

void as_host_bridge_invoke_dispatch() {
	ASHostBridge *bridge = ASHostBridge::get_singleton();
	REQUIRE(bridge != nullptr);
	bridge->uninstall();

	_reset_stub();
	Variant out;

	// 未安装 -> 明确错误，且不触达宿主。
	CHECK(bridge->invoke(1, nullptr, 0, &out) == ERR_UNCONFIGURED);
	CHECK(g_invoke_count == 0);

	const ASHostCallbacks cb = _make_stub();
	REQUIRE(bridge->install(&cb) == OK);

	_reset_stub();
	g_stub_return = Variant(42);
	Variant in = 7;
	CHECK(bridge->invoke(3, &in, 1, &out) == OK);
	CHECK(g_invoke_count == 1);
	CHECK(g_last_method_id == 3);
	CHECK(g_last_argc == 1);
	CHECK(g_last_arg0 == Variant(7));
	CHECK(out == Variant(42));

	// argc == 0 时必须传 nullptr 且宿主仍被调用。
	_reset_stub();
	CHECK(bridge->invoke(4, nullptr, 0, &out) == OK);
	CHECK(g_last_argc == 0);

	// 宿主返回非 0 -> FAILED。
	_reset_stub();
	g_stub_rc = 5;
	CHECK(bridge->invoke(5, nullptr, 0, &out) == FAILED);

	bridge->uninstall();
}

void as_host_bridge_stale_object_is_safe() {
	ASHostBridge *bridge = ASHostBridge::get_singleton();
	REQUIRE(bridge != nullptr);
	bridge->uninstall();

	const ASHostCallbacks cb = _make_stub();
	REQUIRE(bridge->install(&cb) == OK);

	Node *node = memnew(Node);
	const ObjectID id = node->get_instance_id();
	Variant stale = node;
	memdelete(node);
	REQUIRE(ObjectDB::get_instance(id) == nullptr);

	_reset_stub();
	Variant out;
	CHECK(bridge->invoke(6, &stale, 1, &out) == OK);
	CHECK(g_last_arg0_was_invalid_object);

	bridge->uninstall();
}
