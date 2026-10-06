/**************************************************************************/
/*  test_angelscript_lazy_binding.cpp                                     */
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

#include "../binding/as_binding_error_mapper.h"
#include "../binding/as_binding_lazy.h"
#include "../binding/as_binding_plan.h"
#include "../binding/as_binding_registry.h"
#include "../binding/as_binding_scanner.h"

#include <angelscript.h>

#define ANGELSCRIPT_LAZY_BINDING_TESTS_IMPL
#include "test_angelscript_lazy_binding.h"

// 注册守卫按引擎指针记录，释放后地址可能被下一个引擎复用从而误判「已注册」。
// 在测试进程内保留这些引擎可保证地址唯一。
static Vector<asIScriptEngine *> g_retained_engines;

void as_lazy_scanner_intersects_classdb() {
	HashSet<StringName> types;
	ASBindingScanner::scan_source("Node n; Sprite2D s; NotARealClass x;", types);
	CHECK(types.has(StringName("Node")));
	CHECK(types.has(StringName("Sprite2D")));
	CHECK(!types.has(StringName("NotARealClass"))); // 与 ClassDB 求交后丢弃。
}

void as_lazy_scanner_ignores_comments_and_strings() {
	HashSet<StringName> types;
	ASBindingScanner::scan_source(
			"// Node is just a comment\n"
			"const String s = \"Sprite2D inside a string\";\n"
			"Control c;\n",
			types);
	CHECK(types.has(StringName("Control")));
	CHECK(!types.has(StringName("Node")));
	CHECK(!types.has(StringName("Sprite2D")));
}

void as_lazy_register_types_registers_subset_only() {
	ASBindingPlan plan;
	plan.build(ASBindingScope()); // 空 whitelist/blacklist：可见集合为全量。

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	REQUIRE(engine != nullptr);

	CHECK(ASBindingRegistry::register_value_types_and_skeletons(plan, engine) == OK);

	Vector<StringName> subset;
	subset.push_back(StringName("Node")); // Node 的成员签名只引用已建骨架的类型与已注册值类型。
	CHECK(ASBindingRegistry::register_types(plan, subset, engine) == OK);

	// 只注册了 Node 的成员：Node 的方法可见。
	const String src =
			"void probe() {\n"
			"  Node n;\n"
			"  n.get_name();\n"
			"}\n";
	asIScriptModule *mod = engine->GetModule("lazy_subset", asGM_ALWAYS_CREATE);
	REQUIRE(mod != nullptr);
	mod->AddScriptSection("lazy_subset", src.utf8().get_data());
	CHECK(mod->Build() >= 0); // RED：Node 成员未注册时 Build() < 0。

	// 保留引擎不释放：注册守卫按引擎指针记录，释放后地址可能被下一个引擎复用，
	// 从而误判「已注册」。测试进程内保留这些引擎可保证地址唯一。
	g_retained_engines.push_back(engine);
}

void as_lazy_facade_registers_core_and_dedups() {
	ASBindingPlan plan;
	plan.build(ASBindingScope()); // 空 whitelist/blacklist：可见集合为全量。

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	REQUIRE(engine != nullptr);

	ASBindingLazyRegistry::get_singleton()->reset();
	ASBindingLazyRegistry::get_singleton()->ensure_initialized(plan, engine, Vector<String>());

	// core 契约：Object 必须立即可用；但尚未全量。
	CHECK(ASBindingLazyRegistry::get_singleton()->is_registered(StringName("Object")));
	CHECK(!ASBindingLazyRegistry::get_singleton()->is_full());

	// 去重：重复注册同一批类，返回 0（无新增）。
	Vector<StringName> again;
	again.push_back(StringName("Object"));
	CHECK(ASBindingLazyRegistry::get_singleton()->ensure_registered(plan, again, engine) == 0);

	// 未注册且在可见集合内：新增注册，返回 1。
	Vector<StringName> node;
	node.push_back(StringName("Node"));
	CHECK(ASBindingLazyRegistry::get_singleton()->ensure_registered(plan, node, engine) == 1);
	CHECK(ASBindingLazyRegistry::get_singleton()->is_registered(StringName("Node")));

	// 不在可见集合内（不存在于 plan）：静默忽略，返回 0 且不得记入已注册。
	Vector<StringName> unknown;
	unknown.push_back(StringName("ThisClassDoesNotExist"));
	CHECK(ASBindingLazyRegistry::get_singleton()->ensure_registered(plan, unknown, engine) == 0);
	CHECK(!ASBindingLazyRegistry::get_singleton()->is_registered(StringName("ThisClassDoesNotExist")));

	// 已是注册成员：再次请求返回 0（去重）。
	CHECK(ASBindingLazyRegistry::get_singleton()->ensure_registered(plan, node, engine) == 0);

	ASBindingLazyRegistry::get_singleton()->reset();
	g_retained_engines.push_back(engine);
}

void as_lazy_facade_fallback_is_idempotent() {
	// 用白名单收窄可见集合：全量注册测试里约 1047 类会拖慢整套用例，
	// 白名单下的 plan 仍是「完整可见集合」，足以验证兜底语义。
	ASBindingScope scope;
	scope.whitelist.push_back("Node");
	ASBindingPlan plan;
	plan.build(scope);

	asIScriptEngine *engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	REQUIRE(engine != nullptr);

	ASBindingLazyRegistry::get_singleton()->reset();
	ASBindingLazyRegistry::get_singleton()->register_all(plan, engine);
	CHECK(ASBindingLazyRegistry::get_singleton()->is_full());
	CHECK(ASBindingLazyRegistry::get_singleton()->is_registered(StringName("Node")));

	// 兜底后再 ensure_registered 是 no-op（不重复注册、不报错）。
	Vector<StringName> t;
	t.push_back(StringName("Node"));
	CHECK(ASBindingLazyRegistry::get_singleton()->ensure_registered(plan, t, engine) == 0);

	ASBindingLazyRegistry::get_singleton()->reset();
	g_retained_engines.push_back(engine);
}

void as_lazy_error_mapper_extracts_classdb_identifiers() {
	ASBindingPlan plan;
	plan.build(ASBindingScope()); // 空 whitelist/blacklist：可见集合为全量。

	ASBindingMissingSymbols out;
	Vector<String> msgs;
	// 真实文本（探针 Q2b 实证）：未注册类型时 AS 报 `Identifier 'X' is not a data type`，
	// 其中 X 就是可注册的类名。
	msgs.push_back("probe_q2b(2,3): Identifier 'Sprite2D' is not a data type");
	CHECK(ASBindingErrorMapper::extract(msgs, plan, &out));
	REQUIRE(out.types.size() == 1);
	CHECK(out.types[0] == StringName("Sprite2D"));
}

void as_lazy_error_mapper_no_progress_returns_false() {
	ASBindingPlan plan;
	plan.build(ASBindingScope());

	ASBindingMissingSymbols out;
	Vector<String> msgs;
	// 真实文本（探针 Q2 实证）：成员缺失时 AS 只报 `No matching symbol '<member>'`，
	// 消息里没有宿主类名 ⇒ 无法推断归属类，必须返回 false 交由调用方全量兜底。
	msgs.push_back("probe_q2(3,5): No matching symbol 'get_name'");
	CHECK(!ASBindingErrorMapper::extract(msgs, plan, &out));
	CHECK(out.types.is_empty());

	// 可见集合之外的标识符同样不计为进展。
	Vector<String> msgs2;
	msgs2.push_back("probe(1,1): Identifier 'ThisClassDoesNotExist' is not a data type");
	CHECK(!ASBindingErrorMapper::extract(msgs2, plan, &out));
	CHECK(out.types.is_empty());
}
