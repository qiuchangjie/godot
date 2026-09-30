/**************************************************************************/
/*  as_engine.cpp                                                         */
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

#include "as_engine.h"

#include "binding/as_binding_decl.h"
#include "binding/as_binding_registry.h"
#include "core/object/object.h"
#include "core/string/print_string.h"

ASEngine *ASEngine::get_singleton() {
	static ASEngine engine_singleton;
	return &engine_singleton;
}

void ASEngine::_message_callback(const asSMessageInfo *p_msg, void *p_param) {
	ASEngine *self = static_cast<ASEngine *>(p_param);
	if (self == nullptr || p_msg == nullptr) {
		return;
	}
	if (!self->last_error.is_empty()) {
		self->last_error += "\n";
	}
	self->last_error += String(p_msg->section ? p_msg->section : "") + "(" + itos(p_msg->row) + "," + itos(p_msg->col) + "): " + String(p_msg->message ? p_msg->message : "");
}

// 内建函数一律走泛型调用约定（asCALL_GENERIC）：设计 §3 约定绑定层统一使用泛型调用，
// 内建 API 先行遵守，避免 M2 绑定层出现两套调用约定。
static void _as_log_int(asIScriptGeneric *p_generic) {
	// 本阶段只提供 int 版本：字符串类型需要 RegisterStringFactory 与 Godot String 的编组（M2）。
	print_line(itos((int)p_generic->GetArgDWord(0)));
}

// 绑定层完成注册后才存在的调试出口：内建 `print` 是 vararg，AS 2.38 不可表达，
// 因此热更脚本的字符串输出走这里。Godot `String` 值类型以 Variant 为存储，
// 直接复用 AS_KIND_VALUE 编组。
static void _as_log_string(asIScriptGeneric *p_generic) {
	print_line(String(as_binding_marshal_arg(p_generic, 0, AS_KIND_VALUE)));
}

bool ASEngine::ensure_initialized() {
	if (engine != nullptr) {
		return true;
	}

	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if (engine == nullptr) {
		return false;
	}

	engine->SetMessageCallback(asFUNCTION(_message_callback), this, asCALL_CDECL);

	_register_builtins();
	_initialize_binding();
	return true;
}

void ASEngine::_register_builtins() {
	// 阶段一唯一的内建 API：让端到端验收脚本能调用宿主（spec §5、M1 交付项）。
	const int result = engine->RegisterGlobalFunction("void as_log_int(int value)", asFUNCTION(_as_log_int), asCALL_GENERIC);
	ERR_FAIL_COND_MSG(result < 0, vformat("Failed to register the builtin function 'as_log_int' (error %d).", result));
}

void ASEngine::_initialize_binding() {
	// 计划构建会读取 angel_script/class_whitelist 与 class_blacklist（默认全放行）。
	binding_plan.build(ASBindingScope::from_project_settings());
	ASBindingRegistry::register_plan(binding_plan, engine);

	// as_log_string 的签名用到绑定层的 Godot `String` 值类型，必须等到值类型注册之后
	// 才能注册（否则 AS 会因未知类型报错并把引擎标记为配置错误且不可恢复）。
	const int result = engine->RegisterGlobalFunction("void as_log_string(const String &in value)", asFUNCTION(_as_log_string), asCALL_GENERIC);
	ERR_FAIL_COND_MSG(result < 0, vformat("Failed to register the builtin function 'as_log_string' (error %d).", result));
}

bool ASEngine::compile_module(const String &p_name, const String &p_source, String *r_error) {
	ERR_FAIL_NULL_V(engine, false);

	last_error = String();

	// asGM_ALWAYS_CREATE：同名模块整体替换，符合"热更=替换"的语义（spec §7）。
	// 具名 CharString 承载模块名，避免 `p_name.utf8().get_data()` 的临时对象生命周期含糊。
	CharString module_cstr = p_name.utf8();
	asIScriptModule *mod = engine->GetModule(module_cstr.get_data(), asGM_ALWAYS_CREATE);
	if (mod == nullptr) {
		if (r_error) {
			*r_error = "failed to create AngelScript module: " + p_name;
		}
		return false;
	}

	CharString src = p_source.utf8();
	if (mod->AddScriptSection(module_cstr.get_data(), src.get_data(), src.length()) < 0) {
		// asGM_ALWAYS_CREATE 已经替换掉同名旧模块；这里失败必须丢弃新模块，否则会留下一个
		// 没有 section 的孤儿模块（ASScript::compile_source 失败时不会记录 module_name）。
		if (r_error) {
			*r_error = "failed to add script section: " + p_name;
		}
		engine->DiscardModule(module_cstr.get_data());
		return false;
	}

	if (mod->Build() < 0) {
		if (r_error) {
			*r_error = last_error.is_empty() ? "AngelScript build failed: " + p_name : last_error;
		}
		engine->DiscardModule(module_cstr.get_data());
		return false;
	}

	return true;
}

Error ASEngine::execute(asIScriptEngine *p_engine, asIScriptFunction *p_func, int *r_ret) {
	ERR_FAIL_NULL_V(p_engine, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_func, ERR_INVALID_PARAMETER);

	asIScriptContext *ctx = p_engine->CreateContext();
	ERR_FAIL_NULL_V(ctx, ERR_CANT_CREATE);

	// 无参全局函数 = 以空对象、零参调用 call_function，复用其参数编组、返回值映射
	// （bool/int32/int64/float/double）与异常日志；旧实现无条件 GetReturnDWord()，
	// 对 void/bool/double 返回值都会给出错误结果。
	Variant ret;
	const Error err = call_function(ctx, p_func, nullptr, nullptr, 0, &ret);
	ctx->Release();
	if (err != OK) {
		return err;
	}
	if (r_ret != nullptr) {
		*r_ret = (ret.get_type() == Variant::INT) ? (int)(int64_t)ret : 0;
	}
	return OK;
}

void ASEngine::shutdown() {
	if (engine == nullptr) {
		return;
	}
	engine->ShutDownAndRelease();
	engine = nullptr;
	last_error = String();
}

Error ASEngine::call_function(asIScriptContext *p_context, asIScriptFunction *p_func, asIScriptObject *p_object, const Variant **p_args, int p_argc, Variant *r_ret) {
	ERR_FAIL_NULL_V(p_context, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_func, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_argc != (int)p_func->GetParamCount(), ERR_INVALID_PARAMETER);

	if (p_context->Prepare(p_func) < 0) {
		return ERR_CANT_CREATE;
	}
	if (p_object != nullptr) {
		p_context->SetObject(p_object);
	}

	for (int i = 0; i < p_argc; i++) {
		const Variant &arg = *p_args[i];
		// AS 的 SetArg* 会校验形参在栈上的宽度（SetArgQWord 只接受 8 字节形参，
		// 对 32 位 int 会返回 asINVALID_TYPE 并把上下文置为错误态；SetArgFloat/
		// SetArgDouble 分别是 4/8 字节），所以按形参类型 id 选择赋值函数，
		// 而不是只看 Variant 类型，并且必须检查返回值。
		int set_result = asSUCCESS;
		switch (arg.get_type()) {
			case Variant::BOOL:
				set_result = p_context->SetArgByte(i, arg.operator bool() ? 1 : 0);
				break;
			case Variant::INT: {
				int param_type_id = asTYPEID_VOID;
				p_func->GetParam(i, &param_type_id);
				if (param_type_id == asTYPEID_INT64 || param_type_id == asTYPEID_UINT64) {
					set_result = p_context->SetArgQWord(i, (asQWORD)arg.operator int64_t());
				} else {
					set_result = p_context->SetArgDWord(i, (asDWORD)(int32_t)arg.operator int64_t());
				}
			} break;
			case Variant::FLOAT: {
				int param_type_id = asTYPEID_VOID;
				p_func->GetParam(i, &param_type_id);
				if (param_type_id == asTYPEID_FLOAT) {
					set_result = p_context->SetArgFloat(i, (float)arg.operator double());
				} else {
					set_result = p_context->SetArgDouble(i, arg.operator double());
				}
			} break;
			default:
				// 对象/字符串等参数类型留待绑定层（M2）。
				p_context->Unprepare();
				return ERR_INVALID_PARAMETER;
		}
		if (set_result < 0) {
			p_context->Unprepare();
			return ERR_INVALID_PARAMETER;
		}
	}

	const int executed = p_context->Execute();
	if (executed != asEXECUTION_FINISHED) {
		// 静默吞掉异常会让脚本错误在宿主侧完全不可见（callp 只把它翻译成一个泛化错误码）。
		const char *exception = p_context->GetExceptionString();
		const char *section = nullptr;
		const int line = p_context->GetExceptionLineNumber(nullptr, &section);
		ERR_PRINT(vformat("AngelScript call '%s' failed: %s (%s:%d).",
				p_func->GetName() != nullptr ? p_func->GetName() : "<anonymous>",
				exception != nullptr ? exception : "execution did not finish",
				section != nullptr ? section : "<script>",
				line));
		p_context->Unprepare();
		return FAILED;
	}

	if (r_ret != nullptr) {
		switch (p_func->GetReturnTypeId()) {
			case asTYPEID_VOID:
				*r_ret = Variant();
				break;
			case asTYPEID_BOOL:
				*r_ret = Variant(p_context->GetReturnByte() != 0);
				break;
			case asTYPEID_INT32:
				// 32 位返回值必须走 GetReturnDWord：QWord 高位未定义，负数会被读成大正数。
				*r_ret = Variant((int64_t)(int32_t)p_context->GetReturnDWord());
				break;
			case asTYPEID_INT64:
				*r_ret = Variant((int64_t)p_context->GetReturnQWord());
				break;
			case asTYPEID_FLOAT:
			case asTYPEID_DOUBLE:
				*r_ret = Variant(p_context->GetReturnDouble());
				break;
			default:
				*r_ret = Variant();
				break;
		}
	}

	p_context->Unprepare();
	return OK;
}
