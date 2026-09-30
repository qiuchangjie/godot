/**************************************************************************/
/*  test_angelscript_engine.cpp                                           */
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

// 必须先定义该宏再包含测试头：本文件与 tests/test_main.cpp（经 modules_tests.gen.h）
// 都会展开该头，不定义宏就会把 TEST_CASE 注册两遍、用例被跑两次。
#define ANGELSCRIPT_ENGINE_TESTS_IMPL
#include "test_angelscript_engine.h"

#include <angelscript.h>

// 本仓的 doctest 以 DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS 编译（见 tests/test_macros.h）：
// REQUIRE 失败不会中断用例，后面的语句照常执行。所以每个前置条件失败后都显式 return，
// 否则实现一旦退化，用例会以空指针崩溃收场，把同一次运行里其它用例的结果一起吞掉。

void as_engine_lazy_init_is_idempotent() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	if (as == nullptr) {
		return;
	}

	CHECK(as->ensure_initialized());
	REQUIRE(as->get_engine() != nullptr);
	if (as->get_engine() == nullptr) {
		return;
	}

	// 二次调用必须幂等，且拿到同一个引擎实例。
	asIScriptEngine *first = as->get_engine();
	CHECK(as->ensure_initialized());
	CHECK(as->get_engine() == first);
}

void as_engine_compiles_and_executes(const String &p_source, String *r_error, int *r_result) {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	if (as == nullptr) {
		return;
	}

	REQUIRE(as->ensure_initialized());
	if (!as->is_initialized()) {
		return;
	}

	REQUIRE(as->compile_module("as_engine_test", p_source, r_error));

	asIScriptModule *mod = as->get_engine()->GetModule("as_engine_test");
	REQUIRE(mod != nullptr);
	if (mod == nullptr) {
		return;
	}

	asIScriptFunction *func = mod->GetFunctionByDecl("int main()");
	REQUIRE(func != nullptr);
	if (func == nullptr) {
		return;
	}

	int executed_result = 0;
	CHECK(ASEngine::execute(as->get_engine(), func, &executed_result) == OK);
	if (r_result != nullptr) {
		*r_result = executed_result;
	}
}

bool as_engine_compile_module(const String &p_name, const String &p_source, String *r_error) {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as != nullptr);
	if (as == nullptr) {
		return false;
	}

	REQUIRE(as->ensure_initialized());
	if (!as->is_initialized()) {
		return false;
	}

	return as->compile_module(p_name, p_source, r_error);
}
