/**************************************************************************/
/*  test_angelscript_highlighter.h                                        */
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

// 高亮器（含词法器）仅编辑器构建存在，故测试同样只在 TOOLS_ENABLED 下注册。
// 本头被两处 include：tests/test_main.cpp（注册用例）与同目录的
// test_angelscript_highlighter.cpp（拿到函数声明）。实现方必须先定义
// ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL，否则 TEST_CASE 会注册两次。
#ifdef TOOLS_ENABLED

void as_highlighter_lexer_recognizes_keywords_and_builtin_types();
void as_highlighter_lexer_recognizes_literals_comments_and_directives();

#ifndef ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL

TEST_CASE("[AngelScript] highlighter lexer recognizes keywords and builtin types") {
	as_highlighter_lexer_recognizes_keywords_and_builtin_types();
}

TEST_CASE("[AngelScript] highlighter lexer recognizes literals, comments and directives") {
	as_highlighter_lexer_recognizes_literals_comments_and_directives();
}

#endif // ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL

#endif // TOOLS_ENABLED
