/**************************************************************************/
/*  test_angelscript_native_value_ops.h                                   */
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

// 用例注册头：不得 include 任何 AngelScript 头；实现见 test_angelscript_native_value_ops.cpp。

void as_native_vo_scaffold_fallback_unchanged();
void as_native_vo_vector2_opadd();
void as_native_vo_vector2_arith_assign();

#ifndef ANGELSCRIPT_NATIVE_VALUE_OPS_TESTS_IMPL

TEST_CASE("[AngelScript] native value ops scaffold fallback") {
	as_native_vo_scaffold_fallback_unchanged();
}

TEST_CASE("[AngelScript] native value ops vector2 opAdd") {
	as_native_vo_vector2_opadd();
}

TEST_CASE("[AngelScript] native value ops vector2 arith assign") {
	as_native_vo_vector2_arith_assign();
}

#endif // ANGELSCRIPT_NATIVE_VALUE_OPS_TESTS_IMPL
