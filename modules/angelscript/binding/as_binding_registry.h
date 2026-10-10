/**************************************************************************/
/*  as_binding_registry.h                                                 */
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

#include "as_binding_plan.h"

#include "core/error/error_list.h"

#include <angelscript.h>

// 绑定计划 → AS 引擎的唯一注册入口（spec §3.8.2 方案 A）。
// 运行期注册与 .d.as dump 消费同一份 plan，避免两边各写一套内省逻辑而漂移。
class ASBindingRegistry {
public:
	// 全量注册（保持改动前的语义，作为全量兜底与 startup_mode="full" 的路径）。
	static Error register_plan(const ASBindingPlan &p_plan, asIScriptEngine *p_engine);

	// 阶段 A：全部值类型 + 全部对象骨架 + Variant 对象转换构造器。
	// 骨架每类约 3 次 Register* 调用（~1047 类），是子集成员注册安全的前提：
	// 任何成员签名引用的类名此时都已存在，不会因「未知类型」触发不可恢复的 configFailed。
	static Error register_value_types_and_skeletons(const ASBindingPlan &p_plan, asIScriptEngine *p_engine);

	// 阶段 B：只注册 p_types 中列出的类的成员与枚举（幂等由调用方与 ASBindingObject 保证）。
	static Error register_types(const ASBindingPlan &p_plan, const Vector<StringName> &p_types, asIScriptEngine *p_engine);

	// 阶段 C：@GlobalScope 工具函数（只依赖已注册的值类型）。
	static Error register_globals(asIScriptEngine *p_engine);
};
