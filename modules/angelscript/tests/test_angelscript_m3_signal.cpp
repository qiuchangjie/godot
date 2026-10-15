/**************************************************************************/
/*  test_angelscript_m3_signal.cpp                                        */
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
#include "../as_script.h"
#include "../binding/as_binding_object.h"
#include "../binding/as_binding_plan.h"
#include "../binding/as_binding_value_types.h"

#define ANGELSCRIPT_M3_SIGNAL_TESTS_IMPL
#include "test_angelscript_m3_signal.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "scene/main/node.h"

#include <angelscript.h>

namespace {

bool g_bound = false;

// 只绑定探针需要的那条继承链（Object/RefCounted/Resource/Node），避免全量注册。
// 与 test_angelscript_m3_property.cpp 的同名辅助一致：只有全部成功才置位，
// 否则注册失败会被静默吞掉，探针缺失却可能“空跑通过”。
bool ensure_object_binding(asIScriptEngine *p_engine) {
	if (g_bound) {
		return true;
	}

	ASBindingScope scope;
	scope.whitelist.push_back("Object");
	scope.whitelist.push_back("RefCounted");
	scope.whitelist.push_back("Resource");
	scope.whitelist.push_back("Node");

	ASBindingPlan plan;
	plan.build(scope);
	bool ok = true;
	for (const ASBindingClass &c : plan.get_classes()) {
		ok = ASBindingObject::register_skeleton(c, p_engine) == OK && ok;
	}
	for (const ASBindingClass &c : plan.get_classes()) {
		ok = ASBindingObject::register_class(c, p_engine) == OK && ok;
	}
	ok = ASBindingObject::register_enums(plan.get_enums(), p_engine) == OK && ok;

	g_bound = ok;
	return ok;
}

} // namespace

void as_m3_signal_declared_without_instance() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());

	Ref<ASScript> script;
	script.instantiate();
	const String path = "res://m3_signal_demo.as";
	script->set_path(path);

	const String src =
			"// godot_base: Node\n"
			"class m3_signal_demo {\n"
			"	void signal_on_hit(int damage) {}\n"
			"	void signal_empty() {}\n"
			"	void _ready() {}\n"
			"}\n";
	String err;
	REQUIRE_MESSAGE(script->compile_source(src, path, &err), err);

	// 无实例也要可见。
	CHECK(script->has_script_signal("on_hit"));
	CHECK(script->has_script_signal("empty"));
	CHECK_FALSE(script->has_script_signal("_ready"));
	CHECK_FALSE(script->has_script_signal("on_hit_typo"));

	List<MethodInfo> sigs;
	script->get_script_signal_list(&sigs);
	REQUIRE(sigs.size() == 2);

	for (const MethodInfo &mi : sigs) {
		if (String(mi.name) == "on_hit") {
			REQUIRE(mi.arguments.size() == 1);
			CHECK(String(mi.arguments[0].name) == "damage");
			CHECK(mi.arguments[0].type == Variant::INT);
		} else if (String(mi.name) == "empty") {
			CHECK(mi.arguments.is_empty());
		} else {
			CHECK_MESSAGE(false, "unexpected signal name");
		}
	}

	// signal_ 前缀的方法不得出现在普通方法表里。
	List<MethodInfo> methods;
	script->get_script_method_list(&methods);
	for (const MethodInfo &mi : methods) {
		CHECK_FALSE(String(mi.name).begins_with("signal_"));
	}
	CHECK_FALSE(script->has_method("signal_on_hit"));
}

// Task 11：信号「声明 → 发射 → 连接 → 回调」的端到端闭环。
// 分别观察两条送达路径：
//   g_m3_signal_seen        —— 宿主侧静态回调（callable_mp_static）是否被调用；
//   g_m3_signal_script_seen —— 脚本侧 on_ping 是否真的被调用（经宿主探针函数回传）。
// 只断言前者会掩盖缺陷：脚本侧回调走的是「脚本执行中再次进入脚本」的嵌套调用路径，
// 该路径一旦断掉，宿主侧回调仍会照常触发，测试会假绿。
static int g_m3_signal_seen = -1;
static int g_m3_signal_script_seen = -1;

static void _m3_signal_handler(int p_value) {
	g_m3_signal_seen = p_value;
}

static void _probe_signal_received(asIScriptGeneric *p_generic) {
	g_m3_signal_script_seen = (int)p_generic->GetArgDWord(0);
}

void as_m3_signal_emit_delivered() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	ASBindingValueTypes::register_all(engine);
	REQUIRE(ensure_object_binding(engine));

	// 宿主探针：脚本侧 on_ping 被调用时回传参数，用来与宿主侧回调区分开。
	REQUIRE(engine->RegisterGlobalFunction("void probe_signal_received(int value)", asFUNCTION(_probe_signal_received), asCALL_GENERIC) >= 0);

	Ref<ASScript> script;
	script.instantiate();
	const String path = "res://m3_signal_emit.as";
	script->set_path(path);

	// as_self() 取当前脚本实例承载的节点；as_callable() 生成宿主方法 Callable；
	// as_emit_signal() 发射脚本自己声明的信号。
	const String src =
			"// godot_base: Node\n"
			"class m3_signal_emit {\n"
			"	void signal_ping(int value) {}\n"
			"	void _ready() {\n"
			// 注意：Object.connect 的形参是 StringName，字面量 "ping" 是 AS string，
			// 二者之间没有注册隐式转换；而 connect 的第三参 flags 也没有默认值。
			// 故需显式 StringName(...) 并补 0。
			"		as_self().connect(StringName(\"ping\"), as_callable(as_self(), \"on_ping\"), 0);\n"
			"		Array args;\n"
			"		args.push_back(Variant(7));\n"
			"		as_emit_signal(as_self(), \"ping\", args);\n"
			"	}\n"
			"	void on_ping(int value) {\n"
			"		as_log_int(int(value));\n"
			"		probe_signal_received(int(value));\n"
			"	}\n"
			"}\n";
	String err;
	REQUIRE_MESSAGE(script->compile_source(src, path, &err), err);
	if (!script->is_script_valid()) {
		// doctest 无异常：REQUIRE 失败后仍会继续执行，必须显式返回，
		// 否则下面的空指针解引用会把整个测试进程打崩。
		return;
	}

	g_m3_signal_seen = -1;
	g_m3_signal_script_seen = -1;

	Node *owner = memnew(Node);
	owner->set_script(script);
	ScriptInstance *inst = owner->get_script_instance();
	REQUIRE(inst != nullptr);
	if (inst == nullptr) {
		memdelete(owner);
		return;
	}
	REQUIRE(inst->has_method("on_ping"));

	// 宿主侧也接一个回调，验证信号确实分发到宿主。
	owner->connect("ping", callable_mp_static(_m3_signal_handler));

	Callable::CallError ce;
	inst->callp("_ready", nullptr, 0, ce);
	CHECK(ce.error == Callable::CallError::CALL_OK);
	CHECK(g_m3_signal_seen == 7);
	// 脚本侧回调必须真的执行到（嵌套调用路径），而不是只有宿主侧回调收到。
	CHECK(g_m3_signal_script_seen == 7);

	// I-1：信号声明不是可调用方法，实例层也必须隔离（与 ASScript::has_method 一致）。
	CHECK_FALSE(inst->has_method("signal_ping"));
	Variant sig_arg = 1;
	const Variant *sig_args[1] = { &sig_arg };
	Callable::CallError sig_ce;
	inst->callp("signal_ping", sig_args, 1, sig_ce);
	CHECK(sig_ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD);

	memdelete(owner);
}
