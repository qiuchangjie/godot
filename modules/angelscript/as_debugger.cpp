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

#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"

#include <angelscript.h>

namespace {

// 全部 thread_local：AS 可能在工作线程执行，而 ScriptDebugger::debug() 是在
// 「断下的那条线程」上原地阻塞并处理调试消息，后续的 debug_* 回调必然来自同一线程。
// thread_local 正好对齐这一事实，因此不需要加锁。
thread_local asIScriptContext *g_break_context = nullptr;
thread_local String g_break_error;
thread_local bool g_exception_reported = false;

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
