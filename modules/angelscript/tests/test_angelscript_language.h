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
