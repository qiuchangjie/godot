/**************************************************************************/
/*  as_binding_error_mapper.h                                             */
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

#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

// 编译失败消息里「可注册但缺失」的符号集合。
// types：消息中出现的类名（阶段 1 建骨架后，只有未注册成员的类型名才会出现）；
// globals：预留的全局函数/常量名，当前实现不填充（无消费者）。
struct ASBindingMissingSymbols {
	Vector<StringName> types;
	Vector<StringName> globals;
};

// 编译错误驱动的增量注册：从 AS 的编译消息里尽力提取可注册的类型名。
// 刻意「宁可漏、不可错」：只有能匹配到 p_plan 可见类的标识符才算进展；
// 一旦无法提取新符号就返回 false，由调用方走全量兜底（Q4 裁决）。
class ASBindingErrorMapper {
public:
	// 聚合扫描多条消息；返回是否提取到此前未记录的新符号。
	static bool extract(const Vector<String> &p_messages, const ASBindingPlan &p_plan, ASBindingMissingSymbols *r_out);
	// 扫描单条消息。
	static bool extract_from_message(const String &p_message, const ASBindingPlan &p_plan, ASBindingMissingSymbols *r_out);
};
