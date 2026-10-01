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
