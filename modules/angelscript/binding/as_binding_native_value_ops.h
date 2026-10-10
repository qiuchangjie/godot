/**************************************************************************/
/*  as_binding_native_value_ops.h                                         */
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

#include "core/variant/variant.h"

#include <angelscript.h>

// 值类型原生运算符/方法注册入口（P1）。
//
// 命中表项的运算符/方法走原生 C++ thunk，绕过 asCALL_GENERIC 跳板；未命中返回 false，
// 调用方回退既有 generic 路径。存储始终是 Variant（sizeof(Variant)），thunk 只做
// Variant 存储 ↔ 原生值 的转换。
namespace ASNativeValueOps {

bool try_add_op(asIScriptEngine *p_engine, const String &p_type_name, Variant::Type p_type, const String &p_op, const String &p_decl);
bool try_add_method(asIScriptEngine *p_engine, const String &p_type_name, Variant::Type p_type, const StringName &p_method, const String &p_decl);

int native_registration_count();
int native_thunk_calls();
void reset_native_thunk_calls();

// 每个原生 thunk 开头调用，累计 thunk 调用次数（测试用）。
void note_thunk_call();

} // namespace ASNativeValueOps
