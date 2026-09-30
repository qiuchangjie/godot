/**************************************************************************/
/*  test_angelscript_script.cpp                                           */
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

#include "../as_script.h"

// 必须先定义该宏再包含测试头：本文件与 tests/test_main.cpp（经 modules_tests.gen.h）
// 都会展开该头，不定义宏就会把 TEST_CASE 注册两遍、用例被跑两次。
#define ANGELSCRIPT_SCRIPT_TESTS_IMPL
#include "test_angelscript_script.h"

// 本仓的 doctest 以 DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS 编译（见 tests/test_macros.h）：
// REQUIRE 失败不会中断用例，后面的语句照常执行。所以每个前置条件失败后都显式 return，
// 否则实现一旦退化，用例会以空指针崩溃收场，把同一次运行里其它用例的结果一起吞掉。

static Ref<ASScript> _compile(const String &p_source, const String &p_path, String *r_error = nullptr) {
	Ref<ASScript> script;
	script.instantiate();
	String error;
	script->compile_source(p_source, p_path, &error);
	if (r_error) {
		*r_error = error;
	}
	return script;
}

void as_script_parses_base_type_and_class_name() {
	Ref<ASScript> script = _compile(
			"// godot_base: Node\nclass enemy_spawner {\n\tvoid _ready() {}\n}\n",
			"res://enemy_spawner.as");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK(script->is_valid());
	CHECK(script->get_instance_base_type() == StringName("Node"));
	CHECK(script->get_global_name() == StringName()); // D2：本阶段不做全局类名。
	CHECK(script->can_instantiate());
	CHECK(script->has_method("_ready"));
	CHECK_FALSE(script->has_method("_not_defined"));
}

void as_script_derives_class_name_from_path() {
	CHECK(ASScript::get_class_name_for_path("res://a/b/enemy_spawner.as") == "enemy_spawner");
	CHECK(ASScript::get_class_name_for_path("user://patches/1/boss.asb") == "boss");
	CHECK(ASScript::get_class_name_for_path("plain.as") == "plain");
}

void as_script_tolerates_utf8_bom_and_crlf() {
	// BOM 必须用 String::chr(0xFEFF) 显式构造：`String(const char *)` 走 append_latin1
	// （core/string/ustring.h:677），裸字节字面量 "\xEF\xBB\xBF" 会变成 3 个 Latin-1 字符而非
	// 单个 U+FEFF；而真实文件路径（FileAccess::get_as_text → parse_utf8）本来就会跳过 BOM。
	const String source = String::chr(0xFEFF) + String("// godot_base: Node\r\nclass enemy_spawner {\r\n\tvoid _ready() {}\r\n}\r\n");
	Ref<ASScript> script = _compile(source, "res://enemy_spawner.as");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK(script->is_valid());
	CHECK(script->get_instance_base_type() == StringName("Node"));
}

void as_script_rejects_missing_base_directive() {
	String error;
	Ref<ASScript> script = _compile("class enemy_spawner { void _ready() {} }", "res://enemy_spawner.as", &error);
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK_FALSE(script->is_valid());
	CHECK(error.contains("godot_base"));
}

void as_script_rejects_unknown_base_type() {
	String error;
	Ref<ASScript> script = _compile("// godot_base: NotARealGodotClass\nclass enemy_spawner {}", "res://enemy_spawner.as", &error);
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK_FALSE(script->is_valid());
	CHECK_FALSE(error.is_empty());
}

void as_script_rejects_class_name_that_does_not_match_file_name() {
	String error;
	Ref<ASScript> script = _compile("// godot_base: Node\nclass something_else {}", "res://enemy_spawner.as", &error);
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK_FALSE(script->is_valid());
	CHECK_FALSE(error.is_empty());
}

void as_script_rejects_syntax_errors() {
	String error;
	Ref<ASScript> script = _compile("// godot_base: Node\nclass enemy_spawner { void _ready( { } }", "res://enemy_spawner.as", &error);
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK_FALSE(script->is_valid());
	CHECK_FALSE(error.is_empty());
	CHECK(script->instance_create(nullptr) == nullptr);
}
