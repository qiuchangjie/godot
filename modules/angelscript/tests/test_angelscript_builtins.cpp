/**************************************************************************/
/*  test_angelscript_builtins.cpp                                         */
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

#include "../as_engine.h"

#include <angelscript.h>

#define ANGELSCRIPT_BUILTINS_TESTS_IMPL
#include "test_angelscript_builtins.h"

void as_builtin_log_int_is_callable_from_script() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	if (as == nullptr) {
		return;
	}

	REQUIRE(as->ensure_initialized());
	if (!as->is_initialized()) {
		return;
	}

	String error;
	const bool compiled = as->compile_module("as_builtins_test", "void main() { as_log_int(7); }", &error);
	// 编译失败时必须带上 AngelScript 的诊断，否则只看到 false 无从定位（阶段一只注册了 as_log_int）。
	CHECK_MESSAGE(compiled, error);
	if (!compiled) {
		return;
	}

	asIScriptModule *mod = as->get_engine()->GetModule("as_builtins_test");
	REQUIRE(mod != nullptr);
	if (mod == nullptr) {
		return;
	}

	asIScriptFunction *func = mod->GetFunctionByDecl("void main()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		return;
	}

	int unused = 0;
	CHECK(ASEngine::execute(as->get_engine(), func, 0, nullptr, &unused) == OK);
}
