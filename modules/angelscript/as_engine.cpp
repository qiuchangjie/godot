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
