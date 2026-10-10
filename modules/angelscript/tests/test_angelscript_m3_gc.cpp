/**************************************************************************/
/*  test_angelscript_m3_gc.cpp                                            */
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

#define ANGELSCRIPT_M3_GC_TESTS_IMPL
#include "test_angelscript_m3_gc.h"

#include "../binding/as_binding_value_types.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "scene/main/node.h"

void as_m3_gc_throttled() {
	ASEngine *engine = ASEngine::get_singleton();
	REQUIRE(engine != nullptr);
	REQUIRE(engine->ensure_initialized());

	// 间隔为 0 ⇒ 关闭宿主节流：没有显式请求就不得回收。
	engine->set_gc_interval_seconds(0.0);
	engine->maybe_collect_garbage(); // 先消费掉可能存在的 pending 标志
	const int base = engine->get_gc_count();
	engine->maybe_collect_garbage();
	CHECK(engine->get_gc_count() == base);

	// 显式请求 ⇒ 下一次调用必须回收一次。
	engine->request_gc();
	engine->maybe_collect_garbage();
	CHECK(engine->get_gc_count() == base + 1);

	// 请求标志已被消费 ⇒ 不重复回收。
	engine->maybe_collect_garbage();
	CHECK(engine->get_gc_count() == base + 1);

	engine->set_gc_interval_seconds(5.0);
}

void as_m3_instance_dtor_requests_gc() {
	ASEngine *engine = ASEngine::get_singleton();
	REQUIRE(engine != nullptr);
	REQUIRE(engine->ensure_initialized());
	ASBindingValueTypes::register_all(engine->get_engine());

	Ref<ASScript> script;
	script.instantiate();
	const String path = "res://m3_gc_demo.as";
	script->set_path(path);
	const String src =
			"// godot_base: Node\n"
			"class m3_gc_demo {\n"
			"\tvoid _ready() {}\n"
			"}\n";
	String err;
	REQUIRE(script->compile_source(src, path, &err));

	// 关闭时间节流并清空 pending，确保只有析构触发的请求能改变计数。
	engine->set_gc_interval_seconds(0.0);
	engine->maybe_collect_garbage();
	const int base = engine->get_gc_count();

	Node *owner = memnew(Node);
	owner->set_script(script);
	REQUIRE(owner->get_script_instance() != nullptr);
	memdelete(owner); // 析构 ASScriptInstance ⇒ 应置 pending

	engine->maybe_collect_garbage();
	CHECK(engine->get_gc_count() == base + 1);

	engine->set_gc_interval_seconds(5.0);
}

// 探针：AS 对象析构时回调。除了计数，还再次请求回收，用来制造
// 「GarbageCollect 执行期间又产生新请求」这一 I-1 场景。
static int g_gc_collected = 0;
static void _probe_gc_collected(asIScriptGeneric *) {
	g_gc_collected++;
	ASEngine::get_singleton()->request_gc();
}

void as_m3_gc_cycle_collected() {
	ASEngine *engine = ASEngine::get_singleton();
	REQUIRE(engine != nullptr);
	REQUIRE(engine->ensure_initialized());
	asIScriptEngine *as_engine = engine->get_engine();
	ASBindingValueTypes::register_all(as_engine);

	if (as_engine->GetGlobalFunctionByDecl("void probe_gc_collected()") == nullptr) {
		REQUIRE(as_engine->RegisterGlobalFunction("void probe_gc_collected()", asFUNCTION(_probe_gc_collected), asCALL_GENERIC) >= 0);
	}

	// 两个互相引用的 AS 对象：外部句柄离开作用域后形成不可达环，只有 GC 能回收。
	// 析构函数回调宿主探针，从而在 GarbageCollect 内部制造一次「新的回收请求」。
	const String src =
			"class node_a {\n"
			"\tnode_b @other;\n"
			"\t~node_a() { probe_gc_collected(); }\n"
			"}\n"
			"class node_b {\n"
			"\tnode_a @other;\n"
			"\t~node_b() { probe_gc_collected(); }\n"
			"}\n"
			"void build_cycle() {\n"
			"\tnode_a @a = node_a();\n"
			"\tnode_b @b = node_b();\n"
			"\t@a.other = b;\n"
			"\t@b.other = a;\n"
			"}\n";
	String err;
	REQUIRE_MESSAGE(engine->compile_module("m3_gc_cycle_mod", src, &err), err);

	asIScriptModule *mod = as_engine->GetModule("m3_gc_cycle_mod");
	REQUIRE(mod != nullptr);
	asIScriptFunction *build = mod->GetFunctionByDecl("void build_cycle()");
	REQUIRE(build != nullptr);

	// 关闭时间节流并清空 pending，确保计数变化只来自本轮 GC。
	engine->set_gc_interval_seconds(0.0);
	engine->maybe_collect_garbage();
	g_gc_collected = 0;
	const int base = engine->get_gc_count();

	int exec_ret = 0;
	REQUIRE(ASEngine::execute(as_engine, build, &exec_ret) == OK);
	CHECK_MESSAGE(g_gc_collected == 0, "循环引用在回收前不应被析构");

	// 间隔已关，显式请求一次完整回收；本次回收会销毁环并触发析构回调。
	engine->request_gc();

	// M-1：完整回收应销毁环上的两个对象（否则「循环引用可回收」未被真正验证）。
	engine->maybe_collect_garbage();
	CHECK_MESSAGE(g_gc_collected == 2, vformat("环上两个对象都应被回收，实际析构 %d 个", g_gc_collected));
	CHECK(engine->get_gc_count() == base + 1);

	// I-1：析构期间（GarbageCollect 内部）产生的回收请求不能被本次回收吞掉，
	// 必须保留到下一次机会，否则 interval=0 时该请求会永久丢失。
	engine->maybe_collect_garbage();
	CHECK_MESSAGE(engine->get_gc_count() == base + 2, "回收期间新产生的请求被吞掉了");

	engine->set_gc_interval_seconds(5.0);
}
