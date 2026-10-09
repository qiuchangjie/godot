/**************************************************************************/
/*  test_angelscript_debugger.cpp                                         */
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

#include "../as_debugger.h"
#include "../as_engine.h"

#include <angelscript.h>

#define ANGELSCRIPT_DEBUGGER_TESTS_IMPL
#include "test_angelscript_debugger.h"

namespace TestAngelScriptDebugger {

namespace {

// 每个用例用独立模块名，避免 AS 引擎里重名模块互相覆盖。
int g_module_seq = 0;

String next_module_name() {
	return "as_debugger_test_" + itos(g_module_seq++);
}

// 测试专用异常回调的落点：headless 下 EngineDebugger 不活跃，无法走 debug_break，
// 所以测试自己装回调，在「AS 栈仍然活着」的同一时机做断言——与生产路径时机完全一致。
using ProbeFn = void (*)(asIScriptContext *);
ProbeFn g_probe = nullptr;

void probe_callback(asIScriptContext *p_ctx, void *p_user) {
	if (g_probe != nullptr) {
		g_probe(p_ctx);
	}
}

// 编译 p_source，执行其中的 p_func_name，并在异常回调里调用 p_probe。
// 返回 false 表示编译或准备阶段就失败了（调用方应 REQUIRE 后 return）。
bool run_until_exception(const String &p_source, const char *p_func_name, ProbeFn p_probe, String *r_module_name) {
	ASEngine *as = ASEngine::get_singleton();
	if (as == nullptr || !as->ensure_initialized()) {
		return false;
	}

	const String module_name = next_module_name();
	String err;
	if (!as->compile_module(module_name, p_source, &err)) {
		return false;
	}

	asIScriptModule *mod = as->get_engine()->GetModule(module_name.utf8().get_data());
	if (mod == nullptr) {
		return false;
	}
	asIScriptFunction *func = mod->GetFunctionByName(p_func_name);
	if (func == nullptr) {
		return false;
	}

	asIScriptContext *ctx = as->get_engine()->CreateContext();
	if (ctx == nullptr) {
		return false;
	}

	g_probe = p_probe;
	ctx->SetExceptionCallback(asFUNCTION(probe_callback), nullptr, asCALL_CDECL);
	ctx->Prepare(func);
	ctx->Execute();
	g_probe = nullptr;
	ctx->Release();

	if (r_module_name != nullptr) {
		*r_module_name = module_name;
	}
	return true;
}

Vector<ScriptLanguage::StackInfo> g_captured_stack;

void capture_stack(asIScriptContext *p_ctx) {
	g_captured_stack = ASDebugger::build_stack_info(p_ctx);
}

bool g_unhandled_seen = false;

void probe_unhandled_flag(asIScriptContext *p_ctx) {
	g_unhandled_seen = ASDebugger::is_unhandled_exception(p_ctx);
}

List<String> g_local_names;
List<Variant> g_local_values;

void capture_locals(asIScriptContext *p_ctx) {
	g_local_names.clear();
	g_local_values.clear();
	ASDebugger::get_stack_level_locals(p_ctx, 0, &g_local_names, &g_local_values, -1, -1);
}

// 在 names/values 两张平行表里按名字取值；找不到返回空 Variant。
Variant find_local(const String &p_name) {
	const List<String>::Element *n = g_local_names.front();
	const List<Variant>::Element *v = g_local_values.front();
	while (n != nullptr && v != nullptr) {
		if (n->get() == p_name) {
			return v->get();
		}
		n = n->next();
		v = v->next();
	}
	return Variant();
}

} // namespace

void test_builds_stack_info() {
	const String source =
			"void inner() { int d = 0; int x = 1 / d; }\n"
			"void outer() { inner(); }\n";

	g_captured_stack.clear();
	String module_name;
	const bool ran = run_until_exception(source, "outer", capture_stack, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}

	REQUIRE(g_captured_stack.size() >= 2);
	if (g_captured_stack.size() < 2) {
		return;
	}
	CHECK(g_captured_stack[0].func == "inner");
	CHECK(g_captured_stack[1].func == "outer");
	CHECK(g_captured_stack[0].file == module_name);
	CHECK(g_captured_stack[0].line >= 1);
}

void test_skips_caught_exception() {
	// 脚本自己 catch 掉的异常不该惊动调试器——否则用户每写一个 try/catch 都会被断下。
	const String guarded_source =
			"void guarded() {\n"
			"	try { int d = 0; int x = 1 / d; } catch { }\n"
			"}\n";

	g_unhandled_seen = true;
	String module_name;
	bool ran = run_until_exception(guarded_source, "guarded", probe_unhandled_flag, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}
	CHECK_FALSE(g_unhandled_seen);

	// 对照组：没有 try/catch 的同一个异常必须被判为「未处理」，否则上面那条
	// CHECK_FALSE 可能只是因为谓词恒假而碰巧通过。
	const String bare_source =
			"void bare() { int d = 0; int x = 1 / d; }\n";

	g_unhandled_seen = false;
	ran = run_until_exception(bare_source, "bare", probe_unhandled_flag, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}
	CHECK(g_unhandled_seen);
}

void test_reads_locals() {
	// 末尾那个 late 变量在抛异常时还没进入作用域，GetAddressOfVar 会返回 nullptr。
	// 这里顺带确认解码器跳过它而不是解引用空指针崩掉。
	const String source =
			"void probe() {\n"
			"	int a = 42;\n"
			"	double b = 1.5;\n"
			"	bool c = true;\n"
			"	int d = 0;\n"
			"	int boom = 1 / d;\n"
			"	int late = 7;\n"
			"}\n";

	String module_name;
	const bool ran = run_until_exception(source, "probe", capture_locals, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}

	CHECK(find_local("a") == Variant(42));
	CHECK(find_local("b") == Variant(1.5));
	CHECK(find_local("c") == Variant(true));
	CHECK(g_local_names.size() == g_local_values.size());
}

void test_decodes_bound_value_type() {
	const String source =
			"void probe() {\n"
			"	Vector2 v = Vector2(3, 4);\n"
			"	int d = 0;\n"
			"	int boom = 1 / d;\n"
			"}\n";

	String module_name;
	const bool ran = run_until_exception(source, "probe", capture_locals, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}

	const Variant got = find_local("v");
	REQUIRE(got.get_type() == Variant::VECTOR2);
	if (got.get_type() != Variant::VECTOR2) {
		return;
	}
	CHECK(Vector2(got) == Vector2(3, 4));
}

} // namespace TestAngelScriptDebugger
