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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"

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

void as_script_rejects_empty_and_whitespace_source() {
	String error;
	Ref<ASScript> empty = _compile(String(), "res://enemy_spawner.as", &error);
	REQUIRE(empty.is_valid());
	if (!empty.is_valid()) {
		return;
	}
	CHECK_FALSE(empty->is_valid());
	CHECK(error.contains("godot_base"));

	error = String();
	Ref<ASScript> blank = _compile("   \n\t\n  \n", "res://enemy_spawner.as", &error);
	REQUIRE(blank.is_valid());
	if (!blank.is_valid()) {
		return;
	}
	CHECK_FALSE(blank->is_valid());
	CHECK_FALSE(error.is_empty());
}

void as_script_rejects_directive_after_line_10() {
	// 扫描窗口是前 10 行（[0,10)）：第 11 行才出现指令必须被拒绝。
	String source;
	for (int i = 0; i < 10; i++) {
		source += "// filler line " + itos(i) + "\n";
	}
	source += "// godot_base: Node\nclass enemy_spawner {}\n";

	String error;
	Ref<ASScript> script = _compile(source, "res://enemy_spawner.as", &error);
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	CHECK_FALSE(script->is_valid());
	CHECK(error.contains("godot_base"));
}

void as_script_clear_resets_source_code() {
	Ref<ASScript> script = _compile("// godot_base: Node\nclass enemy_spawner {}", "res://enemy_spawner.as");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	REQUIRE(script->has_source_code());

	script->clear();
	CHECK_FALSE(script->is_valid());
	CHECK_FALSE(script->has_source_code());
	CHECK(script->get_source_code().is_empty());
}

void as_script_loads_through_resource_loader_from_user_path() {
	// 全链路：写盘 → ResourceLoader（按 .as 扩展名路由到本模块的 ResourceFormatLoader）→
	// 读回并编译成 ASScript。用 user:// 而非 res://，避免在仓库工作树里留脏文件。
	const String dir = "user://angelscript_full_chain";
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_USERDATA);
	REQUIRE(da.is_valid());
	if (!da.is_valid()) {
		return;
	}
	da->make_dir_recursive(dir);
	const String path = dir + "/hero.as";
	{
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		if (!f.is_valid()) {
			return;
		}
		f->store_string("// godot_base: Node\nclass hero {\n\tvoid _ready() {}\n}\n");
	}

	Error err = OK;
	// CACHE_MODE_IGNORE：本用例只验证加载路径，不把结果留在资源缓存里干扰其它用例。
	Ref<Resource> res = ResourceLoader::load(path, "", ResourceLoader::CACHE_MODE_IGNORE, &err);
	REQUIRE(res.is_valid());
	if (!res.is_valid()) {
		da->remove(path);
		return;
	}
	CHECK(err == OK);

	Ref<ASScript> script = res;
	REQUIRE(script.is_valid());
	if (script.is_valid()) {
		CHECK(script->is_valid());
		CHECK(script->get_instance_base_type() == StringName("Node"));
		// 类名由路径推导（hero.as → hero），源码里的 class hero 必须与之匹配。
		CHECK(script->has_method("_ready"));
	}

	da->remove(path);
}

void as_script_reload_recompiles_from_updated_source() {
	Ref<ASScript> script = _compile("// godot_base: Node\nclass enemy_spawner {\n\tvoid _ready() {}\n}\n", "res://enemy_spawner.as");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}
	REQUIRE(script->is_valid());
	if (!script->is_valid()) {
		return;
	}
	CHECK(script->has_method("_ready"));
	CHECK_FALSE(script->has_method("extra_step"));

	// 模拟热更：替换源码后 reload() 重编。reload() 会把成员 source_code 作为 p_source 别名传入，
	// 若 compile_source() 不先拷贝再 clear()，这里会因 p_source 被清空而编译失败。这是该修复的回归守卫。
	script->set_source_code("// godot_base: Node\nclass enemy_spawner {\n\tvoid _ready() {}\n\tvoid extra_step() {}\n}\n");
	CHECK(script->reload() == OK);
	CHECK(script->is_valid());
	CHECK(script->has_method("extra_step"));
}
