/**************************************************************************/
/*  test_angelscript_debugger.h                                           */
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

namespace TestAngelScriptDebugger {

void test_builds_stack_info();
void test_skips_caught_exception();
void test_reads_locals();
void test_decodes_bound_value_type();
void test_expands_script_class();
void test_decodes_object_handle();

#ifndef ANGELSCRIPT_DEBUGGER_TESTS_IMPL

TEST_CASE("[AngelScript] 调试器能在异常回调内构建调用栈") {
	test_builds_stack_info();
}

TEST_CASE("[AngelScript] 调试器不拦截脚本自己 catch 掉的异常") {
	test_skips_caught_exception();
}

TEST_CASE("[AngelScript] 调试器能读出当前帧的局部标量") {
	test_reads_locals();
}

TEST_CASE("[AngelScript] 调试器能解码绑定值类型局部变量") {
	test_decodes_bound_value_type();
}

TEST_CASE("[AngelScript] 调试器能递归展开脚本类并挡住环与深度") {
	test_expands_script_class();
}

TEST_CASE("[AngelScript] 调试器解码 Godot 对象句柄且容忍对象已销毁") {
	test_decodes_object_handle();
}

#endif

} // namespace TestAngelScriptDebugger
