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

#include "../binding/as_binding_plan.h"
#include "../binding/as_binding_registry.h"
#include "../binding/as_binding_scanner.h"

#include <angelscript.h>

#define ANGELSCRIPT_LAZY_BINDING_TESTS_IMPL
#include "test_angelscript_lazy_binding.h"

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
	static Vector<asIScriptEngine *> g_retained_engines;
	g_retained_engines.push_back(engine);
}
