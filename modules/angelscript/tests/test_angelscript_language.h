/**************************************************************************/
/*  test_angelscript_language.h                                           */
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

#pragma once

#include "tests/test_macros.h"

#include "../as_script_language.h"
#include "../register_types.h"

#include "core/object/script_language.h"

TEST_CASE("[AngelScript] language is registered with ScriptServer") {
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("as");
	REQUIRE(lang != nullptr);
	CHECK(lang->get_name() == "AngelScript");
	CHECK(lang->get_type() == "AngelScript");
	CHECK(lang->get_extension() == "as");
}

// 幂等性：编辑器重启流程可能重复调用 initialize/uninitialize（审阅关注点 3）。
TEST_CASE("[AngelScript] module init/uninit is idempotent") {
	const int count_before = ScriptServer::get_language_count();

	uninitialize_angelscript_module(MODULE_INITIALIZATION_LEVEL_SERVERS);
	uninitialize_angelscript_module(MODULE_INITIALIZATION_LEVEL_SERVERS);
	initialize_angelscript_module(MODULE_INITIALIZATION_LEVEL_SERVERS);
	initialize_angelscript_module(MODULE_INITIALIZATION_LEVEL_SERVERS);

	CHECK(ScriptServer::get_language_count() == count_before);
	CHECK(script_language_as != nullptr);
	REQUIRE(ScriptServer::get_language_for_extension("as") != nullptr);
}

// 保留字列表覆盖 AngelScript 的关键字（漏词只会影响编辑器高亮/补全，不影响编译）。
TEST_CASE("[AngelScript] language declares its reserved words") {
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("as");
	REQUIRE(lang != nullptr);
	if (lang == nullptr) {
		return;
	}
	const Vector<String> words = lang->get_reserved_words();
	CHECK(words.has("catch"));
	CHECK(words.has("explicit"));
	CHECK(words.has("external"));
	CHECK(words.has("get"));
	CHECK(words.has("set"));
	CHECK(words.has("funcdef"));
}

// 编辑器“新建脚本”对话框会调用 make_template()。基类 ScriptLanguage 返回空引用，
// 语言若不重写它，ScriptCreateDialog::_create_new() 会在 scr->set_path() 处解引用空指针而崩溃。
// 该用例锁定“新建脚本不再崩溃，且生成符合 AS 约定的骨架”这一契约。
TEST_CASE("[AngelScript] make_template returns a valid script boilerplate") {
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("as");
	REQUIRE(lang != nullptr);
	if (lang == nullptr) {
		return;
	}

	Ref<Script> scr = lang->make_template(String(), "my_node", "Node");
	REQUIRE(scr.is_valid());
	CHECK(scr->get_language() == lang);

	const String code = scr->get_source_code();
	CHECK(code.contains("// godot_base: Node"));
	CHECK(code.contains("class my_node"));

	// 骨架落盘后必须能通过编译：类名=文件名、基类指令齐全。
	scr->set_path("res://my_node.as");
	CHECK(scr->reload() == OK);
	CHECK(scr->is_valid());
}

// 回归：编辑器给节点挂「刚创建、尚未编译」的 .as 时，Object::set_script() 会请求占位实例。
// 若语言不提供占位实例，脚本就挂不上（Inspector 的 Script 栏为空）。此用例锁定该契约。
TEST_CASE("[AngelScript] placeholder_instance_create provides an instance for uncompiled scripts") {
	ScriptLanguage *lang = ScriptServer::get_language_for_extension("as");
	REQUIRE(lang != nullptr);
	if (lang == nullptr) {
		return;
	}

	Ref<Script> scr = lang->make_template(String(), "ph_node", "Node");
	REQUIRE(scr.is_valid());
	// 新脚本尚未编译、不可实例化——这正是编辑器走占位实例分支的前提。
	CHECK(!scr->can_instantiate());

	Node *owner = memnew(Node);
	PlaceHolderScriptInstance *ph = scr->placeholder_instance_create(owner);
	REQUIRE(ph != nullptr);
	CHECK(ph->is_placeholder());
	CHECK(ph->get_script() == scr);
	CHECK(ph->get_owner() == owner);

	// 占位实例析构会回调脚本的 _placeholder_erased，必须能安全回收。
	memdelete(ph);
	memdelete(owner);
}
