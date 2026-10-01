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
#include "../binding/as_binding_decl.h"
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

	// 只有全部成功才置位：若在注册前就置位，注册失败会被静默吞掉，探针缺失却可能"空跑通过"。
	g_bound = ok;
	return ok;
}

// 探针用全局句柄值（存活 / 已失效两种）；它们是槽内容，不是可解引用的指针。
uint64_t g_live_id = 0;
uint64_t g_stale_id = 0;
// 由真实绑定路径（factory 返回值）交出的槽原值，用来判断槽里到底是指针还是 ObjectID。
uint64_t g_captured_slot = 0;

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

// 抓取真实绑定路径交出的槽原值：参数由 register_skeleton 注册的 factory 返回值填入，
// 不经过本文件任何手工编解码，因此槽里是什么就说明绑定层交出了什么。
void probe_capture_slot(asIScriptGeneric *p_gen) {
	g_captured_slot = (uint64_t)(uintptr_t)p_gen->GetArgObject(0);
	p_gen->SetReturnDWord(0);
}

// 同上，但参数声明成 Object@ —— 用于拿到 RefCounted 对象的 ObjectID
// （RefCounted 的自身槽是裸指针，只有经 Object@ 弱句柄才会折成 ObjectID）。
void probe_capture_object_id(asIScriptGeneric *p_gen) {
	g_captured_slot = (uint64_t)(uintptr_t)p_gen->GetArgObject(0);
	p_gen->SetReturnDWord(0);
}

// 宿主创建 / 释放的探针对象：整条用例共用同一个实例，脚本侧只拿得到非拥有句柄，
// 因此"对象什么时候消失"完全由宿主决定 —— 这正是要验证的失效语义。
Node *g_probe_node = nullptr;

void probe_make_node(asIScriptGeneric *p_gen) {
	if (g_probe_node == nullptr) {
		g_probe_node = memnew(Node);
	}
	p_gen->SetReturnObject(as_handle_encode(g_probe_node, AS_KIND_OBJECT_NONOWNING));
}

void probe_free_node(asIScriptGeneric *p_gen) {
	if (g_probe_node != nullptr) {
		memdelete(g_probe_node);
		g_probe_node = nullptr;
	}
}

// 把参数槽原样当作整数读出（不解码）：用来说明"槽里到底放了什么"。
void probe_handle_raw(asIScriptGeneric *p_gen) {
	p_gen->SetReturnQWord((asQWORD)(uintptr_t)p_gen->GetArgObject(0));
}

// 拥有句柄的槽一定是裸指针，因此可以直接当 Object* 读它的真实 ObjectID，
// 作为"槽里到底该是什么"的参照值（与静态类型无关，永远是对象自身的 ID）。
void probe_owning_id(asIScriptGeneric *p_gen) {
	Object *o = (Object *)p_gen->GetArgObject(0);
	p_gen->SetReturnQWord(o != nullptr ? (asQWORD)(uint64_t)o->get_instance_id() : 0);
}

// 探针函数的幂等注册。以 probe_get_live 是否存在作为守卫：它们必须一次性全部注册，
// 否则同一引擎上先后运行的用例会看到不一致的探针集合。
// 返回注册是否全部成功：注册失败必须让用例失败，而不是让探针缺失后"空跑"。
bool ensure_probe_functions(asIScriptEngine *p_engine) {
	if (p_engine->GetGlobalFunctionByDecl("Node@ probe_get_live()") != nullptr) {
		return true;
	}
	bool ok = true;
	ok = p_engine->RegisterGlobalFunction("Node@ probe_get_live()", asFUNCTION(probe_get_live), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("Node@ probe_get_stale()", asFUNCTION(probe_get_stale), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("int probe_decode(Node@)", asFUNCTION(probe_decode), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("int probe_decode_raw(Node@)", asFUNCTION(probe_decode_raw), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("int probe_capture_slot(Node@)", asFUNCTION(probe_capture_slot), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("Node@ probe_make_node()", asFUNCTION(probe_make_node), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("void probe_free_node()", asFUNCTION(probe_free_node), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("int64 probe_handle_raw(Object@)", asFUNCTION(probe_handle_raw), asCALL_GENERIC) >= 0 && ok;
	// 形参声明走生产渲染器：这条探针因此锁住 as_binding_render_param 对对象句柄的处理。
	const String owning_decl = "int64 probe_owning_id(" + as_binding_render_param("Resource@") + ")";
	ok = p_engine->RegisterGlobalFunction(owning_decl.utf8().get_data(), asFUNCTION(probe_owning_id), asCALL_GENERIC) >= 0 && ok;
	ok = p_engine->RegisterGlobalFunction("int probe_capture_object_id(Object@)", asFUNCTION(probe_capture_object_id), asCALL_GENERIC) >= 0 && ok;
	return ok;
}

// 公共前置：初始化引擎、注册绑定与探针、编译模块、取出入口函数并准备好上下文。
bool prepare_probe_context(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, asIScriptContext **r_ctx, String *r_err) {
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
	if (!ensure_probe_functions(engine)) {
		*r_err = "probe function registration failed";
		return false;
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
	if (ctx->Prepare(func) < 0) {
		*r_err = "Prepare failed";
		ctx->Release();
		return false;
	}
	*r_ctx = ctx;
	return true;
}

bool run_int(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, int64_t *r_out, String *r_err) {
	asIScriptContext *ctx = nullptr;
	if (!prepare_probe_context(p_engine, p_source, p_entry, &ctx, r_err)) {
		return false;
	}
	if (ctx->Execute() != asEXECUTION_FINISHED) {
		*r_err = String(ctx->GetExceptionString() != nullptr ? ctx->GetExceptionString() : "<no exception>");
		ctx->Release();
		return false;
	}
	*r_out = ctx->GetReturnQWord();
	ctx->Release();
	return true;
}

// 期望异常的路径：不做"必须执行成功"的前置判断，而是把原始结果码与异常字符串交回用例，
// 让用例能断言"抛出的是明确异常"而不是静默成功或崩溃。
bool run_int_expecting_exception(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, int *r_code, String *r_exception, String *r_err) {
	asIScriptContext *ctx = nullptr;
	if (!prepare_probe_context(p_engine, p_source, p_entry, &ctx, r_err)) {
		return false;
	}
	*r_code = ctx->Execute();
	*r_exception = ctx->GetExceptionString() != nullptr ? String(ctx->GetExceptionString()) : String();
	ctx->Release();
	return true;
}

bool run_double(asIScriptEngine *p_engine, const String &p_source, const String &p_entry, double *r_out, String *r_err) {
	asIScriptContext *ctx = nullptr;
	if (!prepare_probe_context(p_engine, p_source, p_entry, &ctx, r_err)) {
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

bool nearly(double p_a, double p_b) {
	return p_a > p_b - 0.001 && p_a < p_b + 0.001;
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

// spec §0 R3：句柄的拥有/非拥有语义由**静态类型**决定 ——
// 派生自 RefCounted 的类型其槽里是裸指针（由 AS 的 addref/release 保活），
// 其余类型（含 Object 自身）槽里是 ObjectID，需要在解引用时经 ObjectDB 校验。
void as_m3_property_kind_from_static_type() {
	PropertyInfo pi;
	pi.name = "value";

	// Node 不是 RefCounted ⇒ 非拥有（弱引用）。
	pi.type = Variant::OBJECT;
	pi.class_name = "Node";
	ASBindingType t = ASBindingDecl::resolve(pi);
	REQUIRE(t.valid);
	CHECK(t.kind == AS_KIND_OBJECT_NONOWNING);
	CHECK(t.as_name == "Node@");

	// Resource 派生自 RefCounted ⇒ 拥有（强引用）。
	pi.class_name = "Resource";
	t = ASBindingDecl::resolve(pi);
	REQUIRE(t.valid);
	CHECK(t.kind == AS_KIND_OBJECT_OWNING);
	CHECK(t.as_name == "Resource@");

	// 空 class_name 表示“任意 Object”；Object 自身不是 RefCounted ⇒ 非拥有。
	pi.class_name = StringName();
	t = ASBindingDecl::resolve(pi);
	REQUIRE(t.valid);
	CHECK(t.kind == AS_KIND_OBJECT_NONOWNING);
	CHECK(t.as_name == "Object@");
}

// 通过**真实绑定路径**验证槽语义：Node 由注册好的 factory 创建、句柄槽由绑定层填写。
// 之前的探针是手工注册全局函数自证，这一条才能证明 as_binding_object.cpp 的跳板真的按 R2 交值。
void as_m3_id_slot_through_real_binding() {
	g_captured_slot = 0;
	int64_t out = -1;
	String err;
	const bool ok = run_int(nullptr,
			"int probe() {"
			"  Node @n = Node();"
			"  return probe_capture_slot(n);"
			"}",
			"probe", &out, &err);
	INFO("error: ", err);
	REQUIRE(ok);
	REQUIRE(g_captured_slot != 0);
	// 绑定层交出的必须是 ObjectID —— 能经 ObjectDB 反查回对象。
	// 若交出的仍是裸指针，用指针值当 ObjectID 查表必然查不到（RED 时的表现）。
	CHECK(ObjectDB::get_instance(ObjectID((uint64_t)g_captured_slot)) != nullptr);

	// 收尾：脚本内由 factory 创建的 Node 没有释放通道（非拥有句柄本就不保活），
	// 由宿主按真实类型释放，避免把泄漏带进 ObjectDB 退出报告。
	Node *created = Object::cast_to<Node>(ObjectDB::get_instance(ObjectID((uint64_t)g_captured_slot)));
	if (created != nullptr) {
		memdelete(created);
	}
}

// spec §3.4 与不变量 I-A：非拥有句柄指向的对象被释放后，任何经该句柄的访问
// 都必须得到一条明确的 AS 异常，而不是野指针解引用，也不是静默成功。
// 正控制保证失败确实来自"对象已释放"，而不是这条调用本身就跑不通。
void as_m3_released_entity_rejected() {
	g_probe_node = nullptr;

	// 正控制：不释放时同样的成员调用必须正常返回。
	int64_t out = -1;
	String err;
	const bool positive_ok = run_int(nullptr,
			"int64 probe() {"
			"  Node @n = probe_make_node();"
			"  int64 result = int64(n.get_child_count(false)) + 42;"
			"  probe_free_node();"
			"  return result;"
			"}",
			"probe", &out, &err);
	INFO("positive error: ", err);
	REQUIRE(positive_ok);
	CHECK(out == 42);

	// 负控制：脚本先拿到非拥有句柄，宿主随后释放对象，再次访问必须抛明确异常。
	int code = 0;
	String exception;
	err = String();
	const bool negative_ran = run_int_expecting_exception(nullptr,
			"int64 probe() {"
			"  Node @n = probe_make_node();"
			"  probe_free_node();"
			"  return int64(n.get_child_count(false));"
			"}",
			"probe", &code, &exception, &err);
	INFO("negative error: ", err);
	REQUIRE(negative_ran);
	CHECK(code == asEXECUTION_EXCEPTION);
	// 异常必须点名出错的成员，否则使用者无从定位是哪个句柄失效了（Task 4 的交付点）。
	CHECK(exception.find("get_child_count") >= 0);
	CHECK(exception.find("has been freed") >= 0);
}

// spec §3.3：向上转换必须按**目标** kind 重新编码槽值。
// `Resource@` 是拥有句柄（槽=裸指针），`Object@` 是非拥有句柄（槽=ObjectID）；
// `Object @o = r;` 会走 generic_upcast，必须把裸指针转成 ObjectID，且不得改变引用计数。
void as_m3_weak_handle_slot_holds_id() {
	// 断言 1：转换后非拥有槽里必须是 ObjectID，而不是原封不动的裸指针。
	double slot_delta = 0.0;
	String err;
	const bool slot_ok = run_double(nullptr,
			"double main() {"
			"  Resource @r = Resource();"
			"  int64 id = probe_owning_id(r);"
			"  Object @o = r;"
			"  return double(probe_handle_raw(o) - id);"
			"}",
			"main", &slot_delta, &err);
	INFO("slot error: ", err);
	REQUIRE(slot_ok);
	CHECK(nearly(slot_delta, 0.0));

	// 断言 2：弱引用不参与引用计数 —— 转换前后 RefCounted 的引用数不变。
	double ref_delta = 0.0;
	err = String();
	const bool ref_ok = run_double(nullptr,
			"double main() {"
			"  Resource @r = Resource();"
			"  double base = double(r.get_reference_count());"
			"  Object @o = r;"
			"  return double(r.get_reference_count()) - base;"
			"}",
			"main", &ref_delta, &err);
	INFO("ref error: ", err);
	REQUIRE(ref_ok);
	CHECK(nearly(ref_delta, 0.0));

	// 不变量 3：上面两段脚本创建的 Resource 都必须在上下文结束后真正销毁。
	//   AS 释放句柄时走 generic_release，它必须把计数归零的对象 memdelete 掉。
	//   这里覆盖"只创建 + 上转"的基线路径（参数路径见 as_m3_refcount_paths_balanced 的 ⑦）。
	{
		g_captured_slot = 0;
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double probe() {"
				"  Resource @r = Resource();"
				"  Object @o = r;"
				"  return double(probe_capture_object_id(o));"
				"}",
				"probe", &out, &err);
		INFO("lifetime error: ", err);
		REQUIRE(ok);
		REQUIRE(g_captured_slot != 0);
		RefCounted *leaked = Object::cast_to<RefCounted>(ObjectDB::get_instance(ObjectID((uint64_t)g_captured_slot)));
		INFO("leaked refcount: ", leaked != nullptr ? leaked->get_reference_count() : -1);
		CHECK(leaked == nullptr);
	}
}

// spec §7：引用计数一致性。AS 的 addref 跳板此前只调 init_ref()，
// 对"已经持有其他引用"的对象不会真正加计数，于是 `Resource @b = a;`
// 之后释放 b 会把对象计数打回 0，令 a 变成悬垂句柄。
// 这里按四条可观测路径分别断言，便于 RED 精确定位是哪一条失衡。
void as_m3_refcount_paths_balanced() {
	// ① 新建 + 单个句柄持有 ⇒ 计数为 1。
	{
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double main() {"
				"  Resource @a = Resource();"
				"  return double(a.get_reference_count()) - 1.0;"
				"}",
				"main", &out, &err);
		INFO("base error: ", err);
		REQUIRE(ok);
		CHECK(nearly(out, 0.0));
	}

	// ② 第二个句柄赋值 ⇒ 计数再 +1。
	{
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double main() {"
				"  Resource @a = Resource();"
				"  double base = double(a.get_reference_count());"
				"  Resource @b = a;"
				"  return double(a.get_reference_count()) - base - 1.0;"
				"}",
				"main", &out, &err);
		INFO("assign error: ", err);
		REQUIRE(ok);
		CHECK(nearly(out, 0.0));
	}

	// ③ 第二个句柄置空 ⇒ 归还那一份引用。
	{
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double main() {"
				"  Resource @a = Resource();"
				"  double base = double(a.get_reference_count());"
				"  Resource @b = a;"
				"  @b = null;"
				"  return double(a.get_reference_count()) - base;"
				"}",
				"main", &out, &err);
		INFO("release error: ", err);
		REQUIRE(ok);
		CHECK(nearly(out, 0.0));
	}

	// ④ 参数往返 ⇒ 进出计数守恒（调用期间允许 +1，返回后必须回到原值）。
	{
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"void take(Resource @r) {}"
				"double main() {"
				"  Resource @a = Resource();"
				"  double before = double(a.get_reference_count());"
				"  take(a);"
				"  return double(a.get_reference_count()) - before;"
				"}",
				"main", &out, &err);
		INFO("param error: ", err);
		REQUIRE(ok);
		CHECK(nearly(out, 0.0));
	}

	// ⑤ 返回值路径 ⇒ 接收变量只接管一份引用，不得双重 addref（spec §7 路径 3）。
	//    `duplicate()` 在宿主侧新建对象并 SetReturnObject，若这里再多加一次引用，
	//    返回的计数会变成 2。
	{
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double main() {"
				"  Resource @r = Resource();"
				"  Resource @d = r.duplicate(false);"
				"  return double(d.get_reference_count()) - 1.0;"
				"}",
				"main", &out, &err);
		INFO("return error: ", err);
		REQUIRE(ok);
		CHECK(nearly(out, 0.0));
	}

	// ⑥ 局部 RefCounted 离开作用域后必须真的被释放：引用计数若没回到 0，
	//    对象会永久留在 ObjectDB（进程退出时表现为 leaked 告警）。
	//    这里用 Object@ 弱句柄把对象 ID 带回宿主，上下文结束后再查 ObjectDB。
	{
		g_captured_slot = 0;
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double probe() {"
				"  Resource @a = Resource();"
				"  Object @o = a;"
				"  return double(probe_capture_object_id(o));"
				"}",
				"probe", &out, &err);
		INFO("lifetime error: ", err);
		REQUIRE(ok);
		REQUIRE(g_captured_slot != 0);
		RefCounted *leaked = Object::cast_to<RefCounted>(ObjectDB::get_instance(ObjectID((uint64_t)g_captured_slot)));
		INFO("leaked refcount: ", leaked != nullptr ? leaked->get_reference_count() : -1);
		CHECK(leaked == nullptr);
	}

	// ⑦ 拥有句柄作为**实参**传给已注册的宿主函数 ⇒ 调用前后计数守恒（Ruling C）。
	//    形参声明缺 `+`（auto handle）时，引擎不会在调用后 release 参数，
	//    对象引用计数会永久 +1，上下文结束后仍在 ObjectDB 里残留。
	{
		g_captured_slot = 0;
		double out = 0.0;
		String err;
		const bool ok = run_double(nullptr,
				"double probe() {"
				"  Resource @a = Resource();"
				"  Object @o = a;"
				"  int64 id = probe_owning_id(a);"
				"  return double(probe_capture_object_id(o)) + double(id) * 0.0;"
				"}",
				"probe", &out, &err);
		INFO("owning-arg error: ", err);
		REQUIRE(ok);
		REQUIRE(g_captured_slot != 0);
		RefCounted *leaked = Object::cast_to<RefCounted>(ObjectDB::get_instance(ObjectID((uint64_t)g_captured_slot)));
		INFO("leaked refcount: ", leaked != nullptr ? leaked->get_reference_count() : -1);
		CHECK(leaked == nullptr);
	}
}

// spec §7 与 Ruling C：形参声明渲染器对对象句柄必须追加 `+`（auto handle）。
// 缺 `+` 会让拥有句柄作为实参传给已注册函数时多一次 addref 且无对应 release。
void as_m3_render_param_auto_handle() {
	CHECK(as_binding_render_param("Resource@") == "Resource@+");
	CHECK(as_binding_render_param("Node@") == "Node@+");
	CHECK(as_binding_render_param("Object@") == "Object@+");
	// 既有契约不得回退：标量按值、内建值类型按 const 引用。
	CHECK(as_binding_render_param("int64") == "int64");
	CHECK(as_binding_render_param("bool") == "bool");
	CHECK(as_binding_render_param("double") == "double");
	CHECK(as_binding_render_param("Vector3") == "const Vector3 &in");
}
