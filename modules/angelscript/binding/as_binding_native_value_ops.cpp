/**************************************************************************/
/*  as_binding_native_value_ops.cpp                                       */
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

#include "as_binding_native_value_ops.h"

#include <angelscript.h>

namespace ASNativeValueOps {

static int g_native_registration_count = 0;
static int g_native_thunk_calls = 0;

// 空表：全部回退 generic。T3 起按「类型 + 运算符/方法名」逐条填入原生 thunk。
bool try_add_op(asIScriptEngine *p_engine, const String &p_type_name, Variant::Type p_type, const String &p_op, const String &p_decl) {
	(void)p_engine;
	(void)p_type_name;
	(void)p_type;
	(void)p_op;
	(void)p_decl;
	return false;
}

bool try_add_method(asIScriptEngine *p_engine, const String &p_type_name, Variant::Type p_type, const StringName &p_method, const String &p_decl) {
	(void)p_engine;
	(void)p_type_name;
	(void)p_type;
	(void)p_method;
	(void)p_decl;
	return false;
}

int native_registration_count() { return g_native_registration_count; }
int native_thunk_calls() { return g_native_thunk_calls; }
void reset_native_thunk_calls() { g_native_thunk_calls = 0; }
void note_thunk_call() { g_native_thunk_calls++; }

} // namespace ASNativeValueOps
