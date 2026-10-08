#include "../as_engine.h"
#include "../as_host_bridge.h"
#include "../binding/as_binding_decl.h"

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
Variant g_last_arg1;
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
	g_last_arg1 = (p_argc > 1) ? p_args[1] : Variant();
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
	g_last_arg1 = Variant();
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

namespace {

int64_t g_probe_int = -1;
Variant g_probe_variant;
bool g_probes_registered = false;

void _probe_int(asIScriptGeneric *p_generic) {
	g_probe_int = p_generic->GetArgQWord(0);
}

void _probe_variant(asIScriptGeneric *p_generic) {
	g_probe_variant = as_binding_marshal_arg(p_generic, 0, AS_KIND_VALUE);
}

void _ensure_probes(asIScriptEngine *p_engine) {
	if (g_probes_registered) {
		return;
	}
	p_engine->RegisterGlobalFunction("void probe_int(int64 v)", asFUNCTION(_probe_int), asCALL_GENERIC);
	p_engine->RegisterGlobalFunction("void probe_variant(const Variant &in v)", asFUNCTION(_probe_variant), asCALL_GENERIC);
	g_probes_registered = true;
}

} // namespace

void as_host_call_scalar_roundtrip() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	ASHostBridge *bridge = ASHostBridge::get_singleton();
	_reset_stub();
	g_stub_return = Variant(42);
	const ASHostCallbacks cb_scalar = _make_stub();
	REQUIRE(bridge->install(&cb_scalar) == OK);

	g_probe_int = -1;
	const String src =
			"void run() {\n"
			"	Array a;\n"
			"	a.push_back(Variant(7));\n"
			"	Variant r = as_host_call(1, a);\n"
			"	probe_int(int64(r));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_scalar", src, &error), error);

	asIScriptFunction *func = engine->GetModule("m5_scalar")->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	int ret = 0;
	CHECK(ASEngine::execute(engine, func, &ret) == OK);
	CHECK(g_last_method_id == 1);
	CHECK(g_last_argc == 1);
	CHECK(g_last_arg0 == Variant(7));
	CHECK(g_probe_int == 42);

	bridge->uninstall();
}

void as_host_call_string_and_container_roundtrip() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	ASHostBridge *bridge = ASHostBridge::get_singleton();

	// 字符串入参 + 字符串返回值。
	_reset_stub();
	g_stub_return = Variant(String("pong"));
	const ASHostCallbacks cb_str = _make_stub();
	REQUIRE(bridge->install(&cb_str) == OK);
	g_probe_variant = Variant();
	const String src_str =
			"void run() {\n"
			"	Array a;\n"
			"	a.push_back(Variant(String(\"ping\")));\n"
			"	probe_variant(as_host_call(2, a));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_string", src_str, &error), error);
	asIScriptFunction *fs = engine->GetModule("m5_string")->GetFunctionByDecl("void run()");
	REQUIRE(fs != nullptr);
	int ret = 0;
	CHECK(ASEngine::execute(engine, fs, &ret) == OK);
	CHECK(g_last_arg0 == Variant(String("ping")));
	CHECK(g_probe_variant == Variant(String("pong")));

	// Array 入参 + Dictionary 返回值。
	_reset_stub();
	Dictionary dict;
	dict["k"] = 9;
	g_stub_return = Variant(dict);
	const ASHostCallbacks cb_container = _make_stub();
	REQUIRE(bridge->install(&cb_container) == OK);
	g_probe_variant = Variant();
	const String src_container =
			"void run() {\n"
			"	Array a;\n"
			"	a.push_back(Variant(1));\n"
			"	a.push_back(Variant(2));\n"
			"	probe_variant(as_host_call(3, a));\n"
			"}\n";
	REQUIRE_MESSAGE(as->compile_module("m5_container", src_container, &error), error);
	asIScriptFunction *fc = engine->GetModule("m5_container")->GetFunctionByDecl("void run()");
	REQUIRE(fc != nullptr);
	CHECK(ASEngine::execute(engine, fc, &ret) == OK);
	// 入参 Array 被展平为逐个 Variant 实参（spec §6.1：Variant* args + argc）。
	CHECK(g_last_argc == 2);
	CHECK(g_last_arg0 == Variant(1));
	CHECK(g_last_arg1 == Variant(2));
	REQUIRE(g_probe_variant.get_type() == Variant::DICTIONARY);
	CHECK((Dictionary)g_probe_variant == dict);

	bridge->uninstall();
}

void as_host_call_without_table_reports_error() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	ASHostBridge::get_singleton()->uninstall();

	g_probe_int = -1;
	const String src =
			"void run() {\n"
			"	Array a;\n"
			"	Variant r = as_host_call(1, a);\n"
			"	probe_int(int64(r));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_no_table", src, &error), error);

	asIScriptFunction *func = engine->GetModule("m5_no_table")->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	int ret = 0;
	// 未安装：脚本继续运行，as_host_call 返回空 Variant（转 int64 为 0），不崩溃。
	CHECK(ASEngine::execute(engine, func, &ret) == OK);
	CHECK(g_probe_int == 0);
}
