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

#include <angelscript.h>

namespace {

// 全部 thread_local：AS 可能在工作线程执行，而 ScriptDebugger::debug() 是在
// 「断下的那条线程」上原地阻塞并处理调试消息，后续的 debug_* 回调必然来自同一线程。
// thread_local 正好对齐这一事实，因此不需要加锁。
thread_local asIScriptContext *g_break_context = nullptr;
thread_local String g_break_error;

bool is_level_valid(asIScriptContext *p_ctx, int p_level) {
	return p_ctx != nullptr && p_level >= 0 && p_level < (int)p_ctx->GetCallstackSize();
}

} // namespace

void ASDebugger::attach(asIScriptContext *p_ctx) {
	if (p_ctx == nullptr) {
		return;
	}
	// Task 2 会在这里安装异常回调。
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
