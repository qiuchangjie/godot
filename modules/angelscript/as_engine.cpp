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

#include "core/object/object.h"

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

bool ASEngine::ensure_initialized() {
	if (engine != nullptr) {
		return true;
	}

	engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
	if (engine == nullptr) {
		return false;
	}

	engine->SetMessageCallback(asFUNCTION(_message_callback), this, asCALL_CDECL);
	return true;
}

bool ASEngine::compile_module(const String &p_name, const String &p_source, String *r_error) {
	ERR_FAIL_NULL_V(engine, false);

	last_error = String();

	// asGM_ALWAYS_CREATE：同名模块整体替换，符合"热更=替换"的语义（spec §7）。
	asIScriptModule *mod = engine->GetModule(p_name.utf8().get_data(), asGM_ALWAYS_CREATE);
	if (mod == nullptr) {
		if (r_error) {
			*r_error = "failed to create AngelScript module: " + p_name;
		}
		return false;
	}

	CharString src = p_source.utf8();
	if (mod->AddScriptSection(p_name.utf8().get_data(), src.get_data(), src.length()) < 0) {
		if (r_error) {
			*r_error = "failed to add script section: " + p_name;
		}
		return false;
	}

	if (mod->Build() < 0) {
		if (r_error) {
			*r_error = last_error.is_empty() ? "AngelScript build failed: " + p_name : last_error;
		}
		engine->DiscardModule(p_name.utf8().get_data());
		return false;
	}

	return true;
}

Error ASEngine::execute(asIScriptEngine *p_engine, asIScriptFunction *p_func, int p_argc, void *p_arg_ptrs, int *r_ret) {
	ERR_FAIL_NULL_V(p_engine, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_func, ERR_INVALID_PARAMETER);

	asIScriptContext *ctx = p_engine->CreateContext();
	ERR_FAIL_NULL_V(ctx, ERR_CANT_CREATE);

	int prepared = ctx->Prepare(p_func);
	if (prepared < 0) {
		ctx->Release();
		return ERR_CANT_CREATE;
	}

	(void)p_argc;
	(void)p_arg_ptrs;

	int executed = ctx->Execute();
	if (executed != asEXECUTION_FINISHED) {
		ctx->Release();
		return FAILED;
	}

	if (r_ret) {
		*r_ret = (int)ctx->GetReturnDWord();
	}

	ctx->Unprepare();
	ctx->Release();
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

	if (p_context->Execute() != asEXECUTION_FINISHED) {
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
