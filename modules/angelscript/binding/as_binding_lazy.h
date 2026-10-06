/**************************************************************************/
/*  as_binding_lazy.h                                                     */
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
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

#include <angelscript.h>

// 惰性注册门面：持有「已注册类型」集合，串起 core 预热、阶段 1 扫描、阶段 2 增量与全量兜底。
class ASBindingLazyRegistry {
	static bool full;
	static bool skeleton_done;
	static HashSet<StringName> registered;
	// 门面状态绑定到具体引擎：换引擎（引擎重建、测试）时自动清空，避免把上一个引擎的
	// 已注册状态误用到新引擎上。
	static asIScriptEngine *bound_engine;

public:
	static ASBindingLazyRegistry *get_singleton();

	// 首次初始化：值类型 + 全部对象骨架 + core 成员 + 阶段 1 扫描成员 + @GlobalScope 工具函数。
	void ensure_initialized(const ASBindingPlan &p_plan, asIScriptEngine *p_engine, const Vector<String> &p_project_sources);

	// 增量注册：返回本次新增注册的类数；已 full 时返回 0；参数无法处理返回 -1（调用方兜底）。
	int64_t ensure_registered(const ASBindingPlan &p_plan, const Vector<StringName> &p_types, asIScriptEngine *p_engine);

	// 全量兜底：注册全部类成员与枚举；之后 is_full() 为真。
	void register_all(const ASBindingPlan &p_plan, asIScriptEngine *p_engine);

	bool is_full() const { return full; }
	bool is_registered(const StringName &p_type) const { return full || registered.has(p_type); }
	bool is_skeleton_phase_done() const { return skeleton_done; }

	void reset();

	static bool startup_mode_is_full();
	static bool scan_project_enabled();

private:
	// core：Object 无条件保留（spec §5 可见性规则），加最小基础集。
	static const char *const CORE_TYPES[];
	static const int CORE_TYPE_COUNT;

	// 引擎变化时清空门面状态。
	void _bind_engine(asIScriptEngine *p_engine);
	// 只注册 p_types 中尚未注册、且属于可见集合的类，并把它们记入已注册集合。
	void _register_type_set(const ASBindingPlan &p_plan, const Vector<StringName> &p_types, asIScriptEngine *p_engine);
};
