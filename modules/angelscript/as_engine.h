/**************************************************************************/
/*  as_engine.h                                                           */
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

#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"

#include <angelscript.h>

// AngelScript 引擎的唯一持有者：负责生命周期、模块编译与函数执行。
// 主线程独占（spec §4）；ScriptServer::init_languages() 在 --test / headless 路径下
// 不会被调用，因此这里采用懒初始化。
class ASEngine {
	asIScriptEngine *engine = nullptr;
	String last_error;

	static void _message_callback(const asSMessageInfo *p_msg, void *p_param);

public:
	static ASEngine *get_singleton();

	bool ensure_initialized();
	bool is_initialized() const { return engine != nullptr; }
	asIScriptEngine *get_engine() const { return engine; }

	// 编译一个独立模块；同名模块会被整体替换。失败时把 AngelScript 诊断写入 r_error。
	bool compile_module(const String &p_name, const String &p_source, String *r_error);

	// 执行一个 AS 函数。参数编组在后续任务引入，本阶段仅支持无参函数。
	static Error execute(asIScriptEngine *p_engine, asIScriptFunction *p_func, int p_argc, void *p_arg_ptrs, int *r_ret);

	void shutdown();
};
