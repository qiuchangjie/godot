/**************************************************************************/
/*  test_angelscript_lazy_binding.h                                       */
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

// 用例注册头：不得 include 任何 AngelScript 头；实现见 test_angelscript_lazy_binding.cpp。

void as_lazy_scanner_intersects_classdb();
void as_lazy_scanner_ignores_comments_and_strings();
void as_lazy_register_types_registers_subset_only();
void as_lazy_facade_registers_core_and_dedups();
void as_lazy_facade_fallback_is_idempotent();

#ifndef ANGELSCRIPT_LAZY_BINDING_TESTS_IMPL

TEST_CASE("[AngelScript] lazy scanner intersects ClassDB") {
	as_lazy_scanner_intersects_classdb();
}
TEST_CASE("[AngelScript] lazy scanner ignores comments and strings") {
	as_lazy_scanner_ignores_comments_and_strings();
}
TEST_CASE("[AngelScript] lazy register_types registers subset only") {
	as_lazy_register_types_registers_subset_only();
}
TEST_CASE("[AngelScript] lazy facade registers core and dedups") {
	as_lazy_facade_registers_core_and_dedups();
}
TEST_CASE("[AngelScript] lazy facade fallback registers everything") {
	as_lazy_facade_fallback_is_idempotent();
}

#endif // ANGELSCRIPT_LAZY_BINDING_TESTS_IMPL
