/**************************************************************************/
/*  as_debugger.cpp                                                       */
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

#include "as_debugger.h"

#include "as_script_language.h"

#include "binding/as_binding_decl.h"
#include "binding/as_binding_object.h"

#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"

#include <angelscript.h>

namespace {

// 全部 thread_local：AS 可能在工作线程执行，而 ScriptDebugger::debug() 是在
// 「断下的那条线程」上原地阻塞并处理调试消息，后续的 debug_* 回调必然来自同一线程。
// thread_local 正好对齐这一事实，因此不需要加锁。
thread_local asIScriptContext *g_break_context = nullptr;
thread_local String g_break_error;
thread_local bool g_exception_reported = false;

// 递归展开的默认深度上限。调用方（Godot 调试器）传入 >= 0 的值时以调用方为准。
constexpr int DEFAULT_MAX_DEPTH = 4;

bool is_level_valid(asIScriptContext *p_ctx, int p_level) {
	return p_ctx != nullptr && p_level >= 0 && p_level < (int)p_ctx->GetCallstackSize();
}

} // namespace

void ASDebugger::attach(asIScriptContext *p_ctx) {
	if (p_ctx == nullptr) {
		return;
	}
	// 无条件安装：装一个函数指针本身零成本，是否生效由 on_exception 首行的
	// is_active() 判定。这样就避开了「ASScriptInstance 的上下文在实例构造时就创建、
	// 而调试器可能稍后才激活」的时序耦合。
	p_ctx->SetExceptionCallback(asFUNCTION(ASDebugger::on_exception), nullptr, asCALL_CDECL);
}

bool ASDebugger::is_unhandled_exception(asIScriptContext *p_ctx) {
	if (p_ctx == nullptr) {
		return false;
	}
	// WillExceptionBeCaught() 在异常回调里是可信的：as_context.cpp:5000-5024 显示
	// AS 先算好 m_exceptionWillBeCaught，最后才调回调。
	return !p_ctx->WillExceptionBeCaught();
}

void ASDebugger::on_exception(asIScriptContext *p_ctx, void *p_user) {
	if (!is_unhandled_exception(p_ctx) || !EngineDebugger::is_active()) {
		return;
	}
	// 重入保护：断下期间又抛异常时只上报，不再嵌套阻塞，否则调试器会被锁死。
	const bool reentrant = g_break_context != nullptr;

	asIScriptContext *const prev_context = g_break_context;
	const String prev_error = g_break_error;

	g_break_context = p_ctx;
	const char *exception = p_ctx->GetExceptionString();
	g_break_error = exception != nullptr ? String::utf8(exception) : String("AngelScript exception");

	const Vector<ScriptLanguage::StackInfo> stack = build_stack_info(p_ctx);
	const String func = stack.is_empty() ? String("<anonymous>") : stack[0].func;
	const String file = stack.is_empty() ? String("<script>") : stack[0].file;
	const int line = stack.is_empty() ? -1 : stack[0].line;

	EngineDebugger::get_script_debugger()->send_error(func, file, line,
			"AngelScript Error", g_break_error, true, ERR_HANDLER_SCRIPT, stack);
	g_exception_reported = true;

	if (!reentrant) {
		// p_can_continue = false：AS 异常后执行无法恢复，没有「继续」的语义。
		ASScriptLanguage::get_singleton()->debug_break(g_break_error, false);
	}

	g_break_context = prev_context;
	g_break_error = prev_error;
}

bool ASDebugger::consume_exception_reported() {
	const bool reported = g_exception_reported;
	g_exception_reported = false;
	return reported;
}

asIScriptContext *ASDebugger::get_break_context() {
	return g_break_context;
}

String ASDebugger::get_error() {
	return g_break_error;
}

Vector<ScriptLanguage::StackInfo> ASDebugger::build_stack_info(asIScriptContext *p_ctx) {
	Vector<ScriptLanguage::StackInfo> stack;
	if (p_ctx == nullptr) {
		return stack;
	}

	const int count = (int)p_ctx->GetCallstackSize();
	for (int i = 0; i < count; i++) {
		ScriptLanguage::StackInfo info;
		info.func = get_stack_level_function(p_ctx, i);
		info.file = get_stack_level_source(p_ctx, i);
		info.line = get_stack_level_line(p_ctx, i);
		stack.push_back(info);
	}
	return stack;
}

int ASDebugger::get_stack_level_count() {
	return get_stack_level_count(g_break_context);
}

int ASDebugger::get_stack_level_count(asIScriptContext *p_ctx) {
	if (p_ctx == nullptr) {
		return 0;
	}
	return (int)p_ctx->GetCallstackSize();
}

int ASDebugger::get_stack_level_line(int p_level) {
	return get_stack_level_line(g_break_context, p_level);
}

int ASDebugger::get_stack_level_line(asIScriptContext *p_ctx, int p_level) {
	if (!is_level_valid(p_ctx, p_level)) {
		return -1;
	}
	return p_ctx->GetLineNumber((asUINT)p_level, nullptr, nullptr);
}

String ASDebugger::get_stack_level_function(int p_level) {
	return get_stack_level_function(g_break_context, p_level);
}

String ASDebugger::get_stack_level_function(asIScriptContext *p_ctx, int p_level) {
	if (!is_level_valid(p_ctx, p_level)) {
		return String();
	}
	asIScriptFunction *func = p_ctx->GetFunction((asUINT)p_level);
	if (func == nullptr || func->GetName() == nullptr) {
		return "<anonymous>";
	}
	return String::utf8(func->GetName());
}

String ASDebugger::get_stack_level_source(int p_level) {
	return get_stack_level_source(g_break_context, p_level);
}

String ASDebugger::get_stack_level_source(asIScriptContext *p_ctx, int p_level) {
	if (!is_level_valid(p_ctx, p_level)) {
		return String();
	}
	// section 名就是脚本路径（as_script.cpp:163/291 把 module 名与 AddScriptSection
	// 的名字都设成了 res:// 路径），所以可以直接交给编辑器做点击跳转。
	const char *section = nullptr;
	p_ctx->GetLineNumber((asUINT)p_level, nullptr, &section);
	if (section == nullptr) {
		return "<script>";
	}
	return String::utf8(section);
}

Variant ASDebugger::decode_var(void *p_addr, int p_type_id, asIScriptEngine *p_engine, int p_depth, int p_max_depth, int p_max_subitems, HashSet<const void *> &r_seen) {
	if (p_addr == nullptr || p_engine == nullptr) {
		return Variant();
	}

	// 顺序很重要：必须先按 typeId 判标量，再去查类型名。因为
	// as_name_to_variant_type() 对 "bool"/"int64"/"double" 也会返回非 NIL，
	// 顺序颠倒就会把一个 4 字节 int 当成 Variant 来读，直接读野内存。
	switch (p_type_id) {
		case asTYPEID_VOID:
			return Variant();
		case asTYPEID_BOOL:
			return *reinterpret_cast<const bool *>(p_addr);
		case asTYPEID_INT8:
			return (int64_t)*reinterpret_cast<const int8_t *>(p_addr);
		case asTYPEID_INT16:
			return (int64_t)*reinterpret_cast<const int16_t *>(p_addr);
		case asTYPEID_INT32:
			return (int64_t)*reinterpret_cast<const int32_t *>(p_addr);
		case asTYPEID_INT64:
			return *reinterpret_cast<const int64_t *>(p_addr);
		case asTYPEID_UINT8:
			return (int64_t)*reinterpret_cast<const uint8_t *>(p_addr);
		case asTYPEID_UINT16:
			return (int64_t)*reinterpret_cast<const uint16_t *>(p_addr);
		case asTYPEID_UINT32:
			return (int64_t)*reinterpret_cast<const uint32_t *>(p_addr);
		case asTYPEID_UINT64:
			return (int64_t)*reinterpret_cast<const uint64_t *>(p_addr);
		case asTYPEID_FLOAT:
			return (double)*reinterpret_cast<const float *>(p_addr);
		case asTYPEID_DOUBLE:
			return *reinterpret_cast<const double *>(p_addr);
		default:
			break;
	}

	asITypeInfo *type = p_engine->GetTypeInfoById(p_type_id);
	const String type_name = (type != nullptr && type->GetName() != nullptr) ? String::utf8(type->GetName()) : String();

	const bool is_handle = (p_type_id & asTYPEID_OBJHANDLE) != 0;
	const bool is_script_object = (p_type_id & asTYPEID_SCRIPTOBJECT) != 0;

	// 绑定值类型：34 个内建类型与 Variant 自身在 AS 侧都是独立的 asOBJ_VALUE，
	// 但底层存储统一就是一块 Godot Variant（as_binding_value_types.h:40-43）。
	if (!is_handle && !is_script_object && ASBindingDecl::as_name_to_variant_type(type_name) != Variant::NIL) {
		return *reinterpret_cast<const Variant *>(p_addr);
	}

	// 一次解引用规则：GetAddressOfVar / GetAddressOfProperty 已经替我们解过一层
	// 引用（堆上对象、引用参数），但对句柄明确不解。所以非句柄类型的 p_addr 直指
	// 数据本体，句柄类型的 p_addr 指向句柄槽，槽值才是对象。
	//
	// 先判脚本对象再判句柄：`MyClass @h` 两位会同时置位，顺序反了就会把脚本对象
	// 当成 Godot 对象去喂 as_handle_decode。
	if (is_script_object) {
		asIScriptObject *obj = is_handle
				? *reinterpret_cast<asIScriptObject *const *>(p_addr)
				: reinterpret_cast<asIScriptObject *>(p_addr);
		if (obj == nullptr) {
			return Variant();
		}
		// 深度在「即将往下展开一层」时才判，这样 max_depth = 1 的顶层对象仍能看到
		// 自己的标量成员，而不是整个变成一个 "<...>"。
		if (p_depth >= p_max_depth) {
			return "<...>";
		}
		if (r_seen.has(obj)) {
			return "<cycle>";
		}

		r_seen.insert(obj);
		Dictionary members;
		asITypeInfo *obj_type = obj->GetObjectType();
		members["<class>"] = (obj_type != nullptr && obj_type->GetName() != nullptr)
				? String::utf8(obj_type->GetName())
				: String("<anonymous>");

		const int prop_count = (int)obj->GetPropertyCount();
		const int limit = p_max_subitems >= 0 ? MIN(prop_count, p_max_subitems) : prop_count;
		for (int i = 0; i < limit; i++) {
			const char *prop_name = obj->GetPropertyName((asUINT)i);
			void *prop_addr = obj->GetAddressOfProperty((asUINT)i);
			if (prop_addr == nullptr) {
				continue;
			}
			const String key = prop_name != nullptr ? String::utf8(prop_name) : ("<prop " + itos(i) + ">");
			members[key] = decode_var(prop_addr, obj->GetPropertyTypeId((asUINT)i), p_engine, p_depth + 1, p_max_depth, p_max_subitems, r_seen);
		}
		if (limit < prop_count) {
			members["<truncated>"] = prop_count - limit;
		}
		// 出栈时移除：只禁环，不禁同级出现两次的同一个对象（那是合法的 DAG）。
		r_seen.erase(obj);
		return members;
	}

	// Godot 对象句柄：槽的语义由静态类型是否派生自 RefCounted 决定，
	// 解码必须经 as_handle_decode——OWNING 槽是裸 Object*，NONOWNING 槽是 ObjectID
	// 且需要经 ObjectDB 校验（对象可能已经被 free 掉了）。绝不在这里自己强转。
	if (is_handle && !type_name.is_empty() && ClassDB::class_exists(type_name)) {
		void *slot = *reinterpret_cast<void *const *>(p_addr);
		if (slot == nullptr) {
			return Variant();
		}
		const ASBindingKind kind = ClassDB::is_parent_class(type_name, "RefCounted")
				? AS_KIND_OBJECT_OWNING
				: AS_KIND_OBJECT_NONOWNING;
		Object *obj = as_handle_decode(slot, kind);
		// 对象已销毁时给空 Variant：调试查询发生在程序已经出错的时刻，
		// 二次崩溃会毁掉整个诊断现场。
		return obj != nullptr ? Variant(obj) : Variant();
	}

	const char *decl = p_engine->GetTypeDeclaration(p_type_id, true);
	return "<" + (decl != nullptr ? String::utf8(decl) : String("unknown")) + ">";
}

void ASDebugger::get_stack_level_locals(int p_level, List<String> *p_names, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	get_stack_level_locals(g_break_context, p_level, p_names, p_values, p_max_subitems, p_max_depth);
}

void ASDebugger::get_stack_level_locals(asIScriptContext *p_ctx, int p_level, List<String> *p_names, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (!is_level_valid(p_ctx, p_level) || p_names == nullptr || p_values == nullptr) {
		return;
	}

	asIScriptEngine *engine = p_ctx->GetEngine();
	if (engine == nullptr) {
		return;
	}

	const int depth_limit = p_max_depth >= 0 ? p_max_depth : DEFAULT_MAX_DEPTH;
	const int count = p_ctx->GetVarCount((asUINT)p_level);
	for (int i = 0; i < count; i++) {
		const char *name = nullptr;
		int type_id = 0;
		if (p_ctx->GetVar((asUINT)i, (asUINT)p_level, &name, &type_id) < 0) {
			continue;
		}
		void *addr = p_ctx->GetAddressOfVar((asUINT)i, (asUINT)p_level);
		// 地址为空说明变量还没进入作用域，这时解引用必崩——直接跳过。
		if (addr == nullptr) {
			continue;
		}

		HashSet<const void *> seen;
		p_names->push_back(name != nullptr ? String::utf8(name) : ("<var " + itos(i) + ">"));
		p_values->push_back(decode_var(addr, type_id, engine, 0, depth_limit, p_max_subitems, seen));
	}
}

void ASDebugger::get_stack_level_members(int p_level, List<String> *p_names, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	get_stack_level_members(g_break_context, p_level, p_names, p_values, p_max_subitems, p_max_depth);
}

void ASDebugger::get_stack_level_members(asIScriptContext *p_ctx, int p_level, List<String> *p_names, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (!is_level_valid(p_ctx, p_level) || p_names == nullptr || p_values == nullptr) {
		return;
	}

	asIScriptEngine *engine = p_ctx->GetEngine();
	if (engine == nullptr) {
		return;
	}
	// 全局函数帧没有 this，这时什么都不填。
	asIScriptObject *self = reinterpret_cast<asIScriptObject *>(p_ctx->GetThisPointer((asUINT)p_level));
	if (self == nullptr) {
		return;
	}

	const int depth_limit = p_max_depth >= 0 ? p_max_depth : DEFAULT_MAX_DEPTH;
	const int prop_count = (int)self->GetPropertyCount();
	const int limit = p_max_subitems >= 0 ? MIN(prop_count, p_max_subitems) : prop_count;
	for (int i = 0; i < limit; i++) {
		void *addr = self->GetAddressOfProperty((asUINT)i);
		if (addr == nullptr) {
			continue;
		}
		const char *prop_name = self->GetPropertyName((asUINT)i);

		HashSet<const void *> seen;
		seen.insert(self);
		p_names->push_back(prop_name != nullptr ? String::utf8(prop_name) : ("<prop " + itos(i) + ">"));
		p_values->push_back(decode_var(addr, self->GetPropertyTypeId((asUINT)i), engine, 0, depth_limit, p_max_subitems, seen));
	}
}
