/**************************************************************************/
/*  test_angelscript_binding_object.cpp                                   */
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
#include "../binding/as_binding_object.h"
#include "../binding/as_binding_plan.h"
#include "../binding/as_binding_value_types.h"

#define ANGELSCRIPT_BINDING_OBJECT_TESTS_IMPL
#include "test_angelscript_binding_object.h"

#include "core/string/print_string.h"

#include <angelscript.h>

static bool g_object_bound = false;

// 只绑定测试需要的那条继承链，避免把整个 ClassDB 都挂上去（默认全量留给 T6 的 e2e 验证）。
// 注册必须两遍：先全部骨架，再挂成员，最后枚举；否则跨类引用的签名会解析失败，
// 而 AS 里任何一次 Register* 失败都会永久污染引擎。
static void ensure_object_binding(asIScriptEngine *p_engine) {
	if (g_object_bound) {
		return;
	}
	g_object_bound = true;

	ASBindingScope scope;
	scope.whitelist.push_back("Object");
	scope.whitelist.push_back("RefCounted");
	scope.whitelist.push_back("Resource");
	scope.whitelist.push_back("Node");
	scope.whitelist.push_back("CanvasItem");
	scope.whitelist.push_back("Node2D");
	scope.whitelist.push_back("Sprite2D");

	ASBindingPlan plan;
	plan.build(scope);
	for (const ASBindingClass &c : plan.get_classes()) {
		ASBindingObject::register_skeleton(c, p_engine);
	}
	for (const ASBindingClass &c : plan.get_classes()) {
		ASBindingObject::register_class(c, p_engine);
	}
	ASBindingObject::register_enums(plan.get_enums(), p_engine);
}

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
	ensure_object_binding(engine);

	String err;
	if (!as->compile_module("as_binding_obj_run", p_source, &err)) {
		print_line("compile error: " + err);
		REQUIRE(false);
		return false;
	}
	asIScriptModule *mod = engine->GetModule("as_binding_obj_run");
	if (!mod) {
		REQUIRE(false);
		return false;
	}
	asIScriptFunction *func = mod->GetFunctionByName("main");
	if (!func) {
		REQUIRE(false);
		return false;
	}
	asIScriptContext *ctx = engine->CreateContext();
	if (!ctx) {
		REQUIRE(false);
		return false;
	}
	if (ctx->Prepare(func) < 0) {
		ctx->Release();
		REQUIRE(false);
		return false;
	}
	int rc = ctx->Execute();
	double out = ctx->GetReturnDouble();
	const char *exc = ctx->GetExceptionString();
	int exc_line = ctx->GetExceptionLineNumber();
	ctx->Release();
	if (rc != asEXECUTION_FINISHED) {
		print_line(vformat("runtime error (line %d): %s", exc_line, exc ? exc : "<none>"));
		REQUIRE(false);
		return false;
	}
	if (r_out) {
		*r_out = out;
	}
	return true;
}

void as_binding_object_register_all() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());
	if (!as->is_initialized()) {
		return;
	}
	asIScriptEngine *engine = as->get_engine();
	REQUIRE(ASBindingValueTypes::register_all(engine) == OK);
	ensure_object_binding(engine);

	for (const char *name : { "Object", "RefCounted", "Resource", "Node", "CanvasItem", "Node2D", "Sprite2D" }) {
		CHECK(engine->GetTypeInfoByName(name) != nullptr);
	}
	asITypeInfo *node = engine->GetTypeInfoByName("Node");
	REQUIRE(node != nullptr);
	CHECK(node->GetMethodByName("add_child") != nullptr);
	CHECK(node->GetMethodByName("get_child_count") != nullptr);
	// `name` 是属性而非方法：其访问器被 MethodInfo 的属性排除标志滤掉，只以 virtual property 出现。
	CHECK(node->GetMethodByName("get_name") != nullptr);
	asITypeInfo *node2d = engine->GetTypeInfoByName("Node2D");
	REQUIRE(node2d != nullptr);
	CHECK(node2d->GetMethodByName("get_position") != nullptr);
	CHECK(node2d->GetMethodByName("set_position") != nullptr);
	// 枚举名是类内裸名（如 "ProcessMode"），AS 侧以声明类做前缀：Node_ProcessMode。
	CHECK(engine->GetTypeInfoByName("Node_ProcessMode") != nullptr);
	CHECK(engine->GetGlobalPropertyIndexByName("Object_NOTIFICATION_PREDELETE") >= 0);
}

void as_binding_object_method_call() {
	double out = 0.0;
	if (!run_double(
				"double main() {"
				"  Node @parent = Node();"
				"  Node @child = Node();"
				"  parent.add_child(child, false, 0);"
				"  int64 count = parent.get_child_count(false);"
				"  return count == 1 ? 1.0 : 0.0;"
				"}",
				&out)) {
		return;
	}
	CHECK(nearly(out, 1.0)); // 方法跳板：实参编组 + Object 方法调用 + int64 返回。
	// 注：非 RefCounted 对象在 AS 侧无法释放（Object::free 是 GDVIRTUAL，M2 不绑定），
	// 脚本创建的 Node 会存活到进程退出——这是 M2 记录的已知限制。
}

void as_binding_object_property_access() {
	double out = 0.0;
	if (!run_double(
				"double main() {"
				"  Node2D @n = Node2D();"
				"  Vector2 p(3, 4);"
				"  n.position = p;"
				"  Vector2 q = n.position;"
				"  return (q.x == 3.0 && q.y == 4.0) ? 1.0 : 0.0;"
				"}",
				&out)) {
		return;
	}
	CHECK(nearly(out, 1.0)); // virtual property 读写 + 值类型编组。
}

void as_binding_object_upcast() {
	double out = 0.0;
	if (!run_double(
				"double main() {"
				"  Node @n = Sprite2D();"
				"  Object @o = Node2D();"
				"  bool ok = (n !is null) && (o !is null);"
				"  return ok ? 1.0 : 0.0;"
				"}",
				&out)) {
		return;
	}
	CHECK(nearly(out, 1.0)); // opImplCast：对每个祖先各注册一条。
}

void as_binding_object_refcount_lifetime() {
	double out = 0.0;
	if (!run_double(
				"double main() {"
				"  Resource @r = Resource();"
				"  double a = double(r.get_reference_count());"
				"  Resource @d = r.duplicate(false);"
				"  double b = double(d.get_reference_count());"
				"  @d = null;"
				"  @r = null;"
				"  return (a == 1.0 && b == 1.0) ? 1.0 : 0.0;"
				"}",
				&out)) {
		return;
	}
	// 工厂与“返回 RefCounted 的方法”都必须让脚本恰好持有一份引用：多一份是泄漏，少一份是释放后使用。
	CHECK(nearly(out, 1.0));
}

void as_binding_object_enums_and_constants() {
	double out = 0.0;
	if (!run_double(
				"double main() {"
				"  if (Node_ProcessMode::PROCESS_MODE_ALWAYS != 3) return 0.0;"
				"  if (Object_NOTIFICATION_PREDELETE != 1) return 0.0;"
				"  return 1.0;"
				"}",
				&out)) {
		return;
	}
	CHECK(nearly(out, 1.0)); // ClassDB 枚举（名带点号需净化）+ BIND_CONSTANT 全局常量。
}
