/**************************************************************************/
/*  test_angelscript_m3_property.cpp                                      */
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
#include "../as_script.h"
#include "../binding/as_binding_decl.h"
#include "../binding/as_binding_object.h"
#include "../binding/as_binding_plan.h"
#include "../binding/as_binding_value_types.h"

#define ANGELSCRIPT_M3_PROPERTY_TESTS_IMPL
#include "test_angelscript_m3_property.h"

#include "core/object/class_db.h"
#include "core/os/memory.h"
#include "core/string/print_string.h"
#include "scene/main/node.h"

#include <angelscript.h>

namespace {

bool g_bound = false;
int g_module_seq = 0;

// 最近一次探针运行的失败原因：本文件多用 REQUIRE_MESSAGE 把它显示出来，
// 因为探针任务的合法结局之一是"脚本编译失败"，报错原文是必须记录的结论。
String g_last_error;

// 只绑定探针需要的那条继承链（Object/RefCounted/Resource/Node），避免全量注册。
bool ensure_object_binding(asIScriptEngine *p_engine) {
	if (g_bound) {
		return true;
	}

	ASBindingScope scope;
	scope.whitelist.push_back("Object");
	scope.whitelist.push_back("RefCounted");
	scope.whitelist.push_back("Resource");
	scope.whitelist.push_back("Node");

	ASBindingPlan plan;
	plan.build(scope);
	bool ok = true;
	for (const ASBindingClass &c : plan.get_classes()) {
		ok = ASBindingObject::register_skeleton(c, p_engine) == OK && ok;
	}
	for (const ASBindingClass &c : plan.get_classes()) {
		ok = ASBindingObject::register_class(c, p_engine) == OK && ok;
	}
	ok = ASBindingObject::register_enums(plan.get_enums(), p_engine) == OK && ok;

	// 只有全部成功才置位：否则注册失败会被静默吞掉，探针缺失却可能"空跑通过"。
	g_bound = ok;
	return ok;
}

// 公共前置：初始化引擎、注册绑定、编译模块、取出入口函数并准备好上下文。
bool prepare_context(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, asIScriptContext **r_ctx, String *r_err) {
	ASEngine *as = ASEngine::get_singleton();
	if (!as->ensure_initialized() || !as->is_initialized()) {
		*r_err = "ensure_initialized failed";
		return false;
	}
	asIScriptEngine *engine = p_engine != nullptr ? p_engine : as->get_engine();
	ASBindingValueTypes::register_all(engine);
	if (!ensure_object_binding(engine)) {
		*r_err = "object binding registration failed";
		return false;
	}

	const String module_name = "as_m3_prop_" + itos(g_module_seq++);
	String compile_error;
	if (!as->compile_module(module_name, p_source, &compile_error)) {
		*r_err = compile_error;
		return false;
	}
	asIScriptModule *mod = engine->GetModule(module_name.utf8().get_data());
	ERR_FAIL_NULL_V(mod, false);
	asIScriptFunction *func = mod->GetFunctionByName(p_entry.utf8().get_data());
	if (func == nullptr) {
		*r_err = "entry function not found: " + p_entry;
		return false;
	}
	asIScriptContext *ctx = engine->CreateContext();
	ERR_FAIL_NULL_V(ctx, false);
	if (ctx->Prepare(func) < 0) {
		*r_err = "Prepare failed";
		ctx->Release();
		return false;
	}
	*r_ctx = ctx;
	return true;
}

bool run_double(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, double *r_out, String *r_err) {
	asIScriptContext *ctx = nullptr;
	if (!prepare_context(p_engine, p_source, p_entry, &ctx, r_err)) {
		return false;
	}
	if (ctx->Execute() != asEXECUTION_FINISHED) {
		*r_err = String(ctx->GetExceptionString() != nullptr ? ctx->GetExceptionString() : "<no exception>");
		ctx->Release();
		return false;
	}
	*r_out = ctx->GetReturnDouble();
	ctx->Release();
	return true;
}

// 探针用的简写：入口固定 main、失败原因写进 g_last_error 供 REQUIRE_MESSAGE 显示。
bool run_double(const String &p_source, double *r_out) {
	return run_double(nullptr, p_source, "main", r_out, &g_last_error);
}

bool nearly(double p_a, double p_b) {
	return p_a > p_b - 0.001 && p_a < p_b + 0.001;
}

// 探针 P1：Variant 的 int64 构造 + opImplConv 取回是否可用。
// 形参按 AS_KIND_INT64 取实参，回传 double 便于断言。
void _probe_variant_from_int(asIScriptGeneric *p_gen) {
	p_gen->SetReturnDouble((double)p_gen->GetArgQWord(0));
}

// 探针 P1：int 字面量到 const Variant &in 形参的隐式转换是否可用。
// 形参按 AS_KIND_VALUE 取实参（值类型统一以 Variant 存储）。
void _probe_variant_arg(asIScriptGeneric *p_gen) {
	Variant v = as_binding_marshal_arg(p_gen, 0, AS_KIND_VALUE);
	p_gen->SetReturnDouble(v.is_num() ? (double)v : -1.0);
}

// 探针 P2：脚本侧构造 Array + push_back(Variant) 后能否作为值类型实参传入宿主。
void _probe_array_sum(asIScriptGeneric *p_gen) {
	Variant v = as_binding_marshal_arg(p_gen, 0, AS_KIND_VALUE);
	if (v.get_type() != Variant::ARRAY) {
		p_gen->SetReturnDouble(-1.0);
		return;
	}
	Array a = v;
	double sum = 0.0;
	for (int i = 0; i < a.size(); i++) {
		sum += (double)a[i];
	}
	p_gen->SetReturnDouble(sum);
}

// 探针 P3（Task 8）：脚本侧 `Variant(对象句柄)` 构造出的 Variant 是否持有对象。
void _probe_variant_object(asIScriptGeneric *p_gen) {
	Variant v = as_binding_marshal_arg(p_gen, 0, AS_KIND_VALUE);
	p_gen->SetReturnDouble(v.get_type() == Variant::OBJECT ? 1.0 : 0.0);
}

// 宿主持有的借用对象：脚本只拿非拥有句柄，释放时机完全由宿主决定，
// 避免"脚本创建非 RefCounted 对象却没有释放通道"造成的退出期泄漏。
Node *g_borrow_node = nullptr;

void _probe_borrow_node(asIScriptGeneric *p_gen) {
	if (g_borrow_node == nullptr) {
		g_borrow_node = memnew(Node);
	}
	p_gen->SetReturnObject(as_handle_encode(g_borrow_node, AS_KIND_OBJECT_NONOWNING));
}

} // namespace

void as_m3_probe_array_and_variant() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	REQUIRE(engine != nullptr);
	REQUIRE(ASBindingValueTypes::register_all(engine) == OK);
	REQUIRE(ensure_object_binding(engine));

	REQUIRE(engine->RegisterGlobalFunction("double probe_variant_from_int(int64 v)", asFUNCTION(_probe_variant_from_int), asCALL_GENERIC) >= 0);
	REQUIRE(engine->RegisterGlobalFunction("double probe_variant_arg(const Variant &in v)", asFUNCTION(_probe_variant_arg), asCALL_GENERIC) >= 0);
	REQUIRE(engine->RegisterGlobalFunction("double probe_array_sum(const Array &in a)", asFUNCTION(_probe_array_sum), asCALL_GENERIC) >= 0);

	// 注意：宿主已注册的全局函数对脚本自动可见，脚本中不得再写前向声明，
	// 否则 AngelScript 会报 "Name conflict. 'X' is a global function."。
	const String src =
			"double main() {\n"
			"	Variant v = Variant(42);\n"
			"	Array a;\n"
			"	a.push_back(Variant(1));\n"
			"	a.push_back(Variant(2));\n"
			"	return probe_variant_from_int(int64(v)) + probe_variant_arg(7) + probe_array_sum(a);\n"
			"}\n";

	double out = 0.0;
	REQUIRE_MESSAGE(run_double(src, &out), g_last_error);
	CHECK_MESSAGE(nearly(out, 52.0), vformat("out=%.6f（期望 52）", out)); // 42 + 7 + 3
}

void as_m3_variant_object_arg() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	REQUIRE(engine != nullptr);
	REQUIRE(ASBindingValueTypes::register_all(engine) == OK);
	REQUIRE(ensure_object_binding(engine));

	// `Variant(Object @)` 依赖 Object 类型名：必须在对象骨架注册之后才可解析。
	REQUIRE(ASBindingValueTypes::register_object_conversions(engine) == OK);

	REQUIRE(engine->RegisterGlobalFunction("double probe_variant_object(const Variant &in v)", asFUNCTION(_probe_variant_object), asCALL_GENERIC) >= 0);
	REQUIRE(engine->RegisterGlobalFunction("Node@ probe_borrow_node()", asFUNCTION(_probe_borrow_node), asCALL_GENERIC) >= 0);

	// 宿主已注册的全局函数对脚本自动可见，脚本中不得再写前向声明。
	const String src =
			"double main() {\n"
			"	Node @n = probe_borrow_node();\n"
			"	return probe_variant_object(Variant(n));\n"
			"}\n";

	double out = 0.0;
	const bool ran = run_double(src, &out);
	// 无论成败都由宿主释放（脚本拿的是非拥有句柄，无释放通道）。
	if (g_borrow_node != nullptr) {
		memdelete(g_borrow_node);
		g_borrow_node = nullptr;
	}
	REQUIRE_MESSAGE(ran, g_last_error);
	CHECK_MESSAGE(nearly(out, 1.0), vformat("out=%.6f（期望 1）", out));
}

void as_m3_property_roundtrip() {
	ASEngine *as = ASEngine::get_singleton();
	REQUIRE(as->ensure_initialized());
	asIScriptEngine *engine = as->get_engine();
	REQUIRE(engine != nullptr);
	REQUIRE(ASBindingValueTypes::register_all(engine) == OK);
	REQUIRE(ensure_object_binding(engine));

	Ref<ASScript> script;
	script.instantiate();
	const String path = "res://m3_prop_roundtrip.as";
	script->set_path(path);

	const String src =
			"// godot_base: Node\n"
			"class m3_prop_roundtrip {\n"
			"	Vector2 offset;\n"
			"	String label;\n"
			"	Node @target;\n"
			"}\n";
	String err;
	REQUIRE_MESSAGE(script->compile_source(src, path, &err), err);
	REQUIRE(script->is_script_valid());

	Node *owner = memnew(Node);
	ScriptInstance *inst = script->instance_create(owner);
	REQUIRE(inst != nullptr);
	if (inst == nullptr) {
		memdelete(owner);
		return;
	}
	// 交给 owner 托管：owner 析构时会 memdelete 实例 → ASScriptInstance 析构 → Release AS 对象。
	owner->set_script_instance(inst);

	// 内建值类型属性（AS 侧存储 = 一颗 Variant）。
	const Vector2 v(3, 4);
	CHECK(inst->set("offset", v));
	Variant got;
	REQUIRE(inst->get("offset", got));
	CHECK(got.get_type() == Variant::VECTOR2);
	CHECK(Vector2(got) == v);

	// 同样是 Variant 存储、但脚本写的是 String（大写）。
	CHECK(inst->set("label", String("hi")));
	REQUIRE(inst->get("label", got));
	CHECK(got.get_type() == Variant::STRING);
	CHECK(String(got) == "hi");

	// 对象句柄属性（非拥有：Node 不是 RefCounted）。
	Node *target = memnew(Node);
	CHECK(inst->set("target", target));
	REQUIRE(inst->get("target", got));
	CHECK(got.get_type() == Variant::OBJECT);
	CHECK(got.operator Object *() == target);

	// 类型信息。
	bool valid = false;
	CHECK(inst->get_property_type("offset", &valid) == Variant::VECTOR2);
	CHECK(valid);
	CHECK(inst->get_property_type("target", &valid) == Variant::OBJECT);
	CHECK(valid);

	// I-2：属性列表必须暴露本 PR 新支持的值类型与对象属性（spec T-5 / §4.4）。
	List<PropertyInfo> props;
	inst->get_property_list(&props);
	bool saw_offset = false;
	bool saw_label = false;
	bool saw_target = false;
	for (const PropertyInfo &pi : props) {
		if (pi.name == "offset") {
			saw_offset = true;
			CHECK(pi.type == Variant::VECTOR2);
		} else if (pi.name == "label") {
			saw_label = true;
			CHECK(pi.type == Variant::STRING);
		} else if (pi.name == "target") {
			saw_target = true;
			CHECK(pi.type == Variant::OBJECT);
			CHECK(pi.class_name == StringName("Node"));
		}
	}
	CHECK(saw_offset);
	CHECK(saw_label);
	CHECK(saw_target);

	memdelete(target);
	memdelete(owner); // 触发 ASScriptInstance 析构 → Release AS 对象。
}
