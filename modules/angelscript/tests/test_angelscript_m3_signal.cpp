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

#define ANGELSCRIPT_M3_SIGNAL_TESTS_IMPL
#include "test_angelscript_m3_signal.h"

#include "core/string/print_string.h"

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
