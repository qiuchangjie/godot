/**************************************************************************/
/*  test_angelscript_m3_handle.cpp                                        */
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

#define ANGELSCRIPT_M3_HANDLE_TESTS_IMPL
#include "test_angelscript_m3_handle.h"

#include "core/os/memory.h"
#include "core/object/object.h"
#include "scene/main/node.h"

#include <angelscript.h>

namespace {

bool g_bound = false;
int g_module_seq = 0;

// 只绑定探针需要的那条继承链（Object/RefCounted/Resource/Node），避免全量注册。
void ensure_object_binding(asIScriptEngine *p_engine) {
	if (g_bound) {
		return;
	}
	g_bound = true;

	ASBindingScope scope;
	scope.whitelist.push_back("Object");
	scope.whitelist.push_back("RefCounted");
	scope.whitelist.push_back("Resource");
	scope.whitelist.push_back("Node");

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

// 探针用全局句柄值（存活 / 已失效两种）；它们是槽内容，不是可解引用的指针。
uint64_t g_live_id = 0;
uint64_t g_stale_id = 0;

void probe_get_live(asIScriptGeneric *p_gen) {
	p_gen->SetReturnObject((void *)(uintptr_t)g_live_id);
}

void probe_get_stale(asIScriptGeneric *p_gen) {
	p_gen->SetReturnObject((void *)(uintptr_t)g_stale_id);
}

// 正确解码：槽里存的是 ObjectID，经 ObjectDB 查表校验后才能得到对象。
void probe_decode(asIScriptGeneric *p_gen) {
	const ObjectID id((uint64_t)(uintptr_t)p_gen->GetArgObject(0));
	Object *o = ObjectDB::get_instance(id);
	p_gen->SetReturnDWord(o != nullptr ? 1 : 0);
}

// 负控制：把槽直接当裸指针用（正是 M3 要修掉的错误解释）。只判空、不解引用，可安全运行。
// 它让本用例永久可证伪：一旦有人把非拥有槽当指针用，对已释放对象就会误判为非空。
void probe_decode_raw(asIScriptGeneric *p_gen) {
	p_gen->SetReturnDWord(p_gen->GetArgObject(0) != nullptr ? 1 : 0);
}

bool run_int(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, int64_t *r_out, String *r_err) {
	ASEngine *as = ASEngine::get_singleton();
	if (!as->ensure_initialized() || !as->is_initialized()) {
		*r_err = "ensure_initialized failed";
		return false;
	}
	asIScriptEngine *engine = p_engine != nullptr ? p_engine : as->get_engine();
	ASBindingValueTypes::register_all(engine);
	ensure_object_binding(engine);

	if (engine->GetGlobalFunctionByDecl("Node@ probe_get_live()") == nullptr) {
		engine->RegisterGlobalFunction("Node@ probe_get_live()", asFUNCTION(probe_get_live), asCALL_GENERIC);
		engine->RegisterGlobalFunction("Node@ probe_get_stale()", asFUNCTION(probe_get_stale), asCALL_GENERIC);
		engine->RegisterGlobalFunction("int probe_decode(Node@)", asFUNCTION(probe_decode), asCALL_GENERIC);
		engine->RegisterGlobalFunction("int probe_decode_raw(Node@)", asFUNCTION(probe_decode_raw), asCALL_GENERIC);
	}

	const String module_name = "as_m3_probe_" + itos(g_module_seq++);
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
	if (ctx->Prepare(func) < 0 || ctx->Execute() != asEXECUTION_FINISHED) {
		*r_err = String(ctx->GetExceptionString() ? ctx->GetExceptionString() : "<no exception>");
		ctx->Release();
		return false;
	}
	*r_out = ctx->GetReturnQWord();
	ctx->Release();
	return true;
}

} // namespace

void as_m3_nonowning_id_slot_probe() {
	Node *target = memnew(Node);
	const uint64_t id = (uint64_t)target->get_instance_id();
	g_live_id = id;

	int64_t out = -1;
	String err;

	// 存活期：槽里是 ObjectID。正确解码拿回对象（=1），负控制也非空（=1，因为 ID 非 0）。
	const bool live_ok = run_int(nullptr,
			"int64 probe() {"
			"  Node @n = probe_get_live();"
			"  return probe_decode(n) * 10 + probe_decode_raw(n);"
			"}",
			"probe", &out, &err);
	INFO("live error: ", err);
	REQUIRE(live_ok);
	CHECK(out == 11);

	// 释放后：同一个槽值必须解不出对象（正确解码 = 0）；而"当裸指针用"会误判为非空（= 1）。
	// 这两条合起来就是 R2 的前提：槽里存 ID、且访问必须先经 ObjectDB 校验。
	g_stale_id = id;
	g_live_id = 0;
	memdelete(target);

	out = -1;
	err = String();
	const bool stale_ok = run_int(nullptr,
			"int64 probe() {"
			"  Node @n = probe_get_stale();"
			"  return probe_decode(n) * 10 + probe_decode_raw(n);"
			"}",
			"probe", &out, &err);
	INFO("stale error: ", err);
	REQUIRE(stale_ok);
	CHECK(out == 1);
}
