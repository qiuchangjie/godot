/**************************************************************************/
/*  test_angelscript_host_bridge.cpp                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "../as_engine.h"
#include "../as_host_bridge.h"
#include "../as_script.h"
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
bool g_last_args_was_null = false;
Variant g_stub_return;
int g_stub_rc = 0;

int _stub_invoke(void *p_user_data, int32_t p_method_id, const Variant *p_args, int32_t p_argc, Variant *r_ret) {
	(void)p_user_data;
	g_invoke_count++;
	g_last_method_id = p_method_id;
	g_last_argc = p_argc;
	g_last_args_was_null = (p_args == nullptr);
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
	g_last_args_was_null = false;
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
ObjectID g_stale_probe_id;
bool g_probes_registered = false;

void _probe_int(asIScriptGeneric *p_generic) {
	g_probe_int = p_generic->GetArgQWord(0);
}

void _probe_variant(asIScriptGeneric *p_generic) {
	g_probe_variant = as_binding_marshal_arg(p_generic, 0, AS_KIND_VALUE);
}

// 返回指向 g_stale_probe_id（一个已释放对象）的非拥有句柄，供失效对象跨 AS 边界用例。
void _probe_stale_node(asIScriptGeneric *p_generic) {
	p_generic->SetReturnObject((void *)(uintptr_t)(uint64_t)g_stale_probe_id);
}

void _ensure_probes(asIScriptEngine *p_engine) {
	if (g_probes_registered) {
		return;
	}
	p_engine->RegisterGlobalFunction("void probe_int(int64 v)", asFUNCTION(_probe_int), asCALL_GENERIC);
	p_engine->RegisterGlobalFunction("void probe_variant(const Variant &in v)", asFUNCTION(_probe_variant), asCALL_GENERIC);
	p_engine->RegisterGlobalFunction("Node @probe_stale_node()", asFUNCTION(_probe_stale_node), asCALL_GENERIC);
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

	asIScriptModule *mod_scalar = engine->GetModule("m5_scalar");
	REQUIRE(mod_scalar != nullptr);
	if (mod_scalar == nullptr) {
		bridge->uninstall();
		return;
	}
	asIScriptFunction *func = mod_scalar->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		bridge->uninstall();
		return;
	}
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
	asIScriptModule *mod_str = engine->GetModule("m5_string");
	REQUIRE(mod_str != nullptr);
	if (mod_str == nullptr) {
		bridge->uninstall();
		return;
	}
	asIScriptFunction *fs = mod_str->GetFunctionByDecl("void run()");
	REQUIRE(fs != nullptr);
	if (fs == nullptr) {
		bridge->uninstall();
		return;
	}
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
	asIScriptModule *mod_container = engine->GetModule("m5_container");
	REQUIRE(mod_container != nullptr);
	if (mod_container == nullptr) {
		bridge->uninstall();
		return;
	}
	asIScriptFunction *fc = mod_container->GetFunctionByDecl("void run()");
	REQUIRE(fc != nullptr);
	if (fc == nullptr) {
		bridge->uninstall();
		return;
	}
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

	asIScriptModule *mod_no_table = engine->GetModule("m5_no_table");
	REQUIRE(mod_no_table != nullptr);
	if (mod_no_table == nullptr) {
		return;
	}
	asIScriptFunction *func = mod_no_table->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		return;
	}
	int ret = 0;
	// 未安装：脚本继续运行，as_host_call 返回空 Variant（转 int64 为 0），不崩溃。
	CHECK(ASEngine::execute(engine, func, &ret) == OK);
	CHECK(g_probe_int == 0);
}

namespace {

Callable g_bidi_callable;
// 宿主回调走 fallback 分支（即重入时内层 as_host_call）的次数，用于证明重入真的发生。
int g_bidi_fallback_count = 0;

// id 100 -> 调用 AS 实例方法（host->AS）；其余 id -> 返回 g_stub_return（供重入内层使用）。
int _stub_invoke_bidi(void *p_user_data, int32_t p_method_id, const Variant *p_args, int32_t p_argc, Variant *r_ret) {
	(void)p_user_data;
	if (p_method_id == 100 && g_bidi_callable.is_valid()) {
		Variant arg = (p_argc > 0) ? p_args[0] : Variant();
		Variant out = g_bidi_callable.call(arg);
		if (r_ret != nullptr) {
			*r_ret = out;
		}
		return 0;
	}
	g_bidi_fallback_count++;
	if (r_ret != nullptr) {
		*r_ret = g_stub_return;
	}
	return 0;
}

Variant g_held_object;

// 持有并回传收到的对象元素，用于验证 identity 与生命周期。
int _stub_invoke_object(void *p_user_data, int32_t p_method_id, const Variant *p_args, int32_t p_argc, Variant *r_ret) {
	(void)p_user_data;
	(void)p_method_id;
	if (p_argc > 0) {
		// as_host_call 已把传入 Array 展平：p_args[0] 即脚本传入的元素（spec §6.1）。
		g_held_object = p_args[0];
		if (r_ret != nullptr) {
			*r_ret = p_args[0];
		}
	}
	return 0;
}

ASHostCallbacks _make_bidi_stub() {
	ASHostCallbacks cb;
	cb.abi_version = AS_HOST_BRIDGE_ABI_VERSION;
	cb.struct_size = (uint32_t)sizeof(ASHostCallbacks);
	cb.user_data = nullptr;
	cb.invoke = _stub_invoke_bidi;
	return cb;
}

ASHostCallbacks _make_object_stub() {
	ASHostCallbacks cb;
	cb.abi_version = AS_HOST_BRIDGE_ABI_VERSION;
	cb.struct_size = (uint32_t)sizeof(ASHostCallbacks);
	cb.user_data = nullptr;
	cb.invoke = _stub_invoke_object;
	return cb;
}

} // namespace

void as_host_call_bidirectional_roundtrip() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	Ref<ASScript> script;
	script.instantiate();
	const String src =
			"// godot_base: Node\n"
			"class m5_bidi {\n"
			"	int on_echo(int x) { return x * 2; }\n"
			"	int on_reenter(int x) {\n"
			"		Array a;\n"
			"		a.push_back(Variant(x));\n"
			"		return int64(as_host_call(1, a)) + 1000;\n"
			"	}\n"
			"	void run_probe() {\n"
			"		Array a;\n"
			"		a.push_back(Variant(21));\n"
			"		probe_int(int64(as_host_call(100, a)));\n"
			"	}\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(script->compile_source(src, "res://m5_bidi.as", &error), error);

	Node *owner = memnew(Node);
	owner->set_script(script);
	ScriptInstance *inst = owner->get_script_instance();
	REQUIRE(inst != nullptr);
	if (inst == nullptr) {
		g_bidi_callable = Callable();
		memdelete(owner);
		return;
	}

	ASHostBridge *bridge = ASHostBridge::get_singleton();
	Callable::CallError ce;

	// 宿主->AS：stub 调用 AS 实例方法并把结果回传脚本。
	const ASHostCallbacks cb_bidi = _make_bidi_stub();
	REQUIRE(bridge->install(&cb_bidi) == OK);
	g_bidi_callable = Callable(owner, StringName("on_echo"));
	g_probe_int = -1;
	inst->callp(StringName("run_probe"), nullptr, 0, ce);
	CHECK(g_probe_int == 42);

	// 重入：AS->宿主->AS->宿主->AS（on_reenter 内层返回 g_stub_return=7，外层 +1000）。
	// 断言 1007 与 fallback 计数，确保内层 as_host_call 真的发生（否则外层只得到 7）。
	g_bidi_callable = Callable(owner, StringName("on_reenter"));
	g_stub_return = Variant(7);
	g_bidi_fallback_count = 0;
	g_probe_int = -1;
	inst->callp(StringName("run_probe"), nullptr, 0, ce);
	CHECK(g_probe_int == 1007);
	CHECK(g_bidi_fallback_count == 1);

	g_bidi_callable = Callable();
	bridge->uninstall();
	memdelete(owner);
}

void as_host_call_object_roundtrip_and_lifetime() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	g_held_object = Variant();
	g_probe_variant = Variant();

	ASHostBridge *bridge = ASHostBridge::get_singleton();
	const ASHostCallbacks cb_object = _make_object_stub();
	REQUIRE(bridge->install(&cb_object) == OK);

	const String src =
			"void run() {\n"
			"	Array a;\n"
			"	RefCounted @obj = RefCounted();\n"
			"	a.push_back(Variant(obj));\n"
			"	probe_variant(as_host_call(200, a));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_object", src, &error), error);

	asIScriptModule *mod_object = engine->GetModule("m5_object");
	REQUIRE(mod_object != nullptr);
	if (mod_object == nullptr) {
		g_held_object = Variant();
		g_probe_variant = Variant();
		bridge->uninstall();
		return;
	}
	asIScriptFunction *func = mod_object->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		g_held_object = Variant();
		g_probe_variant = Variant();
		bridge->uninstall();
		return;
	}
	int ret = 0;
	CHECK(ASEngine::execute(engine, func, &ret) == OK);

	Object *held = g_held_object.get_validated_object();
	REQUIRE(held != nullptr);
	if (held == nullptr) {
		g_held_object = Variant();
		g_probe_variant = Variant();
		bridge->uninstall();
		return;
	}
	const ObjectID held_id = held->get_instance_id();
	RefCounted *rc = Object::cast_to<RefCounted>(held);
	REQUIRE(rc != nullptr);
	if (rc == nullptr) {
		g_held_object = Variant();
		g_probe_variant = Variant();
		bridge->uninstall();
		return;
	}

	// identity：宿主回传的对象与脚本传入的是同一个。
	CHECK(g_probe_variant.get_validated_object() == held);
	// 脚本作用域结束后，C++ 侧持有的 Variant 仍在续命。
	CHECK(rc->get_reference_count() >= 1);

	g_probe_variant = Variant();
	CHECK(ObjectDB::get_instance(held_id) != nullptr); // g_held_object 仍续命
	g_held_object = Variant();
	CHECK(ObjectDB::get_instance(held_id) == nullptr); // 全部释放后销毁

	bridge->uninstall();
}

void as_host_call_empty_array_reaches_host() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	ASHostBridge *bridge = ASHostBridge::get_singleton();
	_reset_stub();
	g_stub_return = Variant(5);
	const ASHostCallbacks cb_empty = _make_stub();
	REQUIRE(bridge->install(&cb_empty) == OK);

	g_probe_int = -1;
	const String src =
			"void run() {\n"
			"	Array a;\n"
			"	probe_int(int64(as_host_call(9, a)));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_empty", src, &error), error);

	asIScriptModule *mod_empty = engine->GetModule("m5_empty");
	REQUIRE(mod_empty != nullptr);
	if (mod_empty == nullptr) {
		bridge->uninstall();
		return;
	}
	asIScriptFunction *func = mod_empty->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		bridge->uninstall();
		return;
	}
	int ret = 0;
	CHECK(ASEngine::execute(engine, func, &ret) == OK);
	// 空 Array 展平为零参：宿主收到 argc==0 且 args==nullptr（spec §4/§6.1）。
	CHECK(g_invoke_count == 1);
	CHECK(g_last_method_id == 9);
	CHECK(g_last_argc == 0);
	CHECK(g_last_args_was_null);
	CHECK(g_probe_int == 5);

	bridge->uninstall();
}

void as_host_call_stale_object_reaches_host() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	_ensure_probes(engine);

	ASHostBridge *bridge = ASHostBridge::get_singleton();
	_reset_stub();
	const ASHostCallbacks cb_stale = _make_stub();
	REQUIRE(bridge->install(&cb_stale) == OK);

	// 造一个已释放的 Node，并让探针交出指向它的非拥有（失效）句柄。
	Node *stale = memnew(Node);
	g_stale_probe_id = stale->get_instance_id();
	memdelete(stale);
	REQUIRE(ObjectDB::get_instance(g_stale_probe_id) == nullptr);

	const String src =
			"void run() {\n"
			"	Node @n = probe_stale_node();\n"
			"	Array a;\n"
			"	a.push_back(Variant(n));\n"
			"	probe_variant(as_host_call(30, a));\n"
			"}\n";
	String error;
	REQUIRE_MESSAGE(as->compile_module("m5_stale", src, &error), error);

	asIScriptModule *mod_stale = engine->GetModule("m5_stale");
	REQUIRE(mod_stale != nullptr);
	if (mod_stale == nullptr) {
		bridge->uninstall();
		return;
	}
	asIScriptFunction *func = mod_stale->GetFunctionByDecl("void run()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		bridge->uninstall();
		return;
	}
	int ret = 0;
	CHECK(ASEngine::execute(engine, func, &ret) == OK);
	// 失效句柄经 AS 编码→Variant→桥 到达宿主时已解析为空对象，不崩溃（spec §6.4）。
	CHECK(g_invoke_count == 1);
	CHECK(g_last_method_id == 30);
	CHECK(g_last_argc == 1);
	CHECK(g_last_arg0.get_validated_object() == nullptr);

	bridge->uninstall();
}
