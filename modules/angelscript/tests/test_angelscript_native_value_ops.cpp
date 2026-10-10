/**************************************************************************/
/*  test_angelscript_native_value_ops.cpp                                 */
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
#include "../binding/as_binding_native_value_ops.h"
#include "../binding/as_binding_value_types.h"

#define ANGELSCRIPT_NATIVE_VALUE_OPS_TESTS_IMPL
#include "test_angelscript_native_value_ops.h"

#include "core/string/print_string.h"

#include <angelscript.h>

static bool nearly(double p_a, double p_b) {
	return p_a > p_b - 0.001 && p_a < p_b + 0.001;
}

// 编译并执行 `double main()`，取回其 double 返回值。
static bool run_double(const String &p_source, double *r_out) {
	ASEngine *as = ASEngine::get_singleton();
	if (!as->ensure_initialized() || !as->is_initialized()) {
		REQUIRE(false);
		return false;
	}
	asIScriptEngine *engine = as->get_engine();
	if (ASBindingValueTypes::register_all(engine) != OK) {
		REQUIRE(false);
		return false;
	}
	String err;
	if (!as->compile_module("as_native_vo_run", p_source, &err)) {
		print_line("compile error: " + err);
		REQUIRE(false);
		return false;
	}
	asIScriptModule *mod = engine->GetModule("as_native_vo_run");
	asIScriptFunction *func = mod ? mod->GetFunctionByName("main") : nullptr;
	if (!func) {
		REQUIRE(false);
		return false;
	}
	asIScriptContext *ctx = engine->CreateContext();
	if (!ctx || ctx->Prepare(func) < 0) {
		if (ctx) {
			ctx->Release();
		}
		REQUIRE(false);
		return false;
	}
	int rc = ctx->Execute();
	double out = ctx->GetReturnDouble();
	ctx->Release();
	if (rc != asEXECUTION_FINISHED) {
		REQUIRE(false);
		return false;
	}
	if (r_out) {
		*r_out = out;
	}
	return true;
}

void as_native_vo_scaffold_fallback_unchanged() {
	double out = 0.0;
	if (!run_double("double main() { Vector2 a(1, 2); Vector2 b(3, 4); Vector2 c = a + b; return c.x + c.y; }", &out)) {
		return;
	}
	CHECK(nearly(out, 10.0));
}

void as_native_vo_vector2_opadd() {
	double out = 0.0;
	ASNativeValueOps::reset_native_thunk_calls();
	if (!run_double("double main() { Vector2 a(1, 2); Vector2 b(3, 4); Vector2 c = a + b; return c.x * 10.0 + c.y; }", &out)) {
		return;
	}
	CHECK(nearly(out, 46.0)); // (1+3)*10 + (2+4)
	CHECK(ASNativeValueOps::native_thunk_calls() > 0); // 证明走了原生 opAdd
}
