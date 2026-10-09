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
#include "../binding/as_binding_object.h"

#include "core/object/object.h"

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

int g_locals_max_depth = -1;

void capture_locals_with_depth(asIScriptContext *p_ctx) {
	g_local_names.clear();
	g_local_values.clear();
	ASDebugger::get_stack_level_locals(p_ctx, 0, &g_local_names, &g_local_values, -1, g_locals_max_depth);
}

// 非空时在解码之前把它 free 掉，用来模拟「调试器查看变量时对象已经没了」。
Object *g_free_before_decode = nullptr;

void probe_object_handle(asIScriptContext *p_ctx) {
	if (g_free_before_decode != nullptr) {
		memdelete(g_free_before_decode);
		g_free_before_decode = nullptr;
	}
	capture_locals(p_ctx);
}

// 带一个 Object@ 实参调用脚本函数。ASEngine::call_function 目前只编组标量实参
// （as_engine.cpp 的 default 分支直接返回 ERR_INVALID_PARAMETER），所以这里自己
// 按绑定层的槽语义编码句柄再 SetArgAddress——与绑定层产出的槽完全同构。
// 返回 false 表示编译/查找阶段就失败了。
bool run_with_object_arg(const String &p_source, Object *p_arg, ProbeFn p_probe) {
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
	asIScriptFunction *func = mod->GetFunctionByName("probe");
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
	// Object 不派生自 RefCounted，槽里放的是 ObjectID 而不是裸指针。
	ctx->SetArgAddress(0, as_handle_encode(p_arg, AS_KIND_OBJECT_NONOWNING));
	ctx->Execute();

	g_probe = nullptr;
	ctx->Release();
	return true;
}

// 对一个给定的上下文，用越界的层号把全部查询跑一遍，断言都返回安全默认值。
void expect_safe_defaults(asIScriptContext *p_ctx, int p_level) {
	CHECK(ASDebugger::get_stack_level_line(p_ctx, p_level) == -1);
	CHECK(ASDebugger::get_stack_level_function(p_ctx, p_level) == String());
	CHECK(ASDebugger::get_stack_level_source(p_ctx, p_level) == String());
	CHECK(ASDebugger::parse_stack_level_expression(p_ctx, p_level, "anything") == String());

	List<String> names;
	List<Variant> values;
	ASDebugger::get_stack_level_locals(p_ctx, p_level, &names, &values, -1, -1);
	CHECK(names.is_empty());
	CHECK(values.is_empty());

	ASDebugger::get_stack_level_members(p_ctx, p_level, &names, &values, -1, -1);
	CHECK(names.is_empty());
	CHECK(values.is_empty());
}

void probe_invalid_levels(asIScriptContext *p_ctx) {
	expect_safe_defaults(p_ctx, -1);
	expect_safe_defaults(p_ctx, 999);
	// 合法层上的未知表达式同样要返回空串，而不是瞎猜。
	CHECK(ASDebugger::parse_stack_level_expression(p_ctx, 0, "no_such_name") == String());
	CHECK(ASDebugger::parse_stack_level_expression(p_ctx, 0, "a") == String("42"));
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

void test_expands_script_class() {
	// node.next 指回 node 自身，构造一个环。解码器必须在第二次遇到它时返回 "<cycle>"，
	// 而不是无限递归把栈撑爆。
	const String source =
			"class Node2 {\n"
			"	int id;\n"
			"	double weight;\n"
			"	Node2 @next;\n"
			"}\n"
			"void probe() {\n"
			"	Node2 node;\n"
			"	node.id = 7;\n"
			"	node.weight = 2.5;\n"
			"	@node.next = node;\n"
			"	int d = 0;\n"
			"	int boom = 1 / d;\n"
			"}\n";

	g_locals_max_depth = -1;
	String module_name;
	bool ran = run_until_exception(source, "probe", capture_locals_with_depth, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}

	const Variant got = find_local("node");
	REQUIRE(got.get_type() == Variant::DICTIONARY);
	if (got.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Dictionary dict = got;
	CHECK(dict.has("<class>"));
	CHECK(String(dict["<class>"]) == "Node2");
	CHECK(dict["id"] == Variant(7));
	CHECK(dict["weight"] == Variant(2.5));

	REQUIRE(dict.has("next"));
	if (!dict.has("next")) {
		return;
	}
	// 环上的那一跳：next 指回 node，必须被环检测拦住。
	CHECK(dict["next"] == Variant("<cycle>"));

	// 深度上限：max_depth = 1 时顶层标量仍是真值，但不再往下展开一层对象。
	g_locals_max_depth = 1;
	g_local_names.clear();
	g_local_values.clear();
	ran = run_until_exception(source, "probe", capture_locals_with_depth, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}
	const Variant shallow = find_local("node");
	REQUIRE(shallow.get_type() == Variant::DICTIONARY);
	if (shallow.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Dictionary shallow_dict = shallow;
	CHECK(shallow_dict["id"] == Variant(7));
	CHECK(shallow_dict["next"] == Variant("<...>"));
}

void test_decodes_object_handle() {
	// Object 不派生自 RefCounted，所以句柄槽里放的是 ObjectID 而不是裸指针。
	// 这条用例同时钉住两件事：槽只能解一层引用、且必须经 as_handle_decode。
	const String source =
			"void probe(Object @o) {\n"
			"	int d = 0;\n"
			"	int boom = 1 / d;\n"
			"}\n";

	Object *alive = memnew(Object);
	g_free_before_decode = nullptr;
	bool ran = run_with_object_arg(source, alive, probe_object_handle);
	REQUIRE(ran);
	if (!ran) {
		memdelete(alive);
		return;
	}
	const Variant got = find_local("o");
	CHECK(got.get_type() == Variant::OBJECT);
	CHECK(got.get_validated_object() == alive);
	memdelete(alive);

	// 对象在解码之前被销毁：必须给空 Variant，绝不能二次崩溃毁掉诊断现场。
	Object *doomed = memnew(Object);
	g_free_before_decode = doomed;
	ran = run_with_object_arg(source, doomed, probe_object_handle);
	REQUIRE(ran);
	if (!ran) {
		return;
	}
	CHECK(find_local("o").get_validated_object() == nullptr);
}

void test_handles_invalid_level() {
	// 没有断点时（break_context == nullptr），所有无参查询必须给安全默认值。
	CHECK(ASDebugger::get_break_context() == nullptr);
	CHECK(ASDebugger::get_stack_level_count() == 0);
	CHECK(ASDebugger::get_stack_level_line(0) == -1);
	CHECK(ASDebugger::get_stack_level_function(0) == String());
	CHECK(ASDebugger::get_stack_level_source(0) == String());
	CHECK(ASDebugger::parse_stack_level_expression(0, "a") == String());

	List<String> names;
	List<Variant> values;
	ASDebugger::get_stack_level_locals(0, &names, &values, -1, -1);
	CHECK(names.is_empty());
	ASDebugger::get_stack_level_members(0, &names, &values, -1, -1);
	CHECK(names.is_empty());

	expect_safe_defaults(nullptr, 0);

	const String source =
			"void probe() {\n"
			"	int a = 42;\n"
			"	int d = 0;\n"
			"	int boom = 1 / d;\n"
			"}\n";

	String module_name;
	const bool ran = run_until_exception(source, "probe", probe_invalid_levels, &module_name);
	REQUIRE(ran);
	if (!ran) {
		return;
	}
}

} // namespace TestAngelScriptDebugger
