/**************************************************************************/
/*  as_binding_lazy.cpp                                                   */
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

#include "as_binding_lazy.h"

#include "as_binding_registry.h"
#include "as_binding_scanner.h"

#include "core/config/project_settings.h"

bool ASBindingLazyRegistry::full = false;
bool ASBindingLazyRegistry::skeleton_done = false;
HashSet<StringName> ASBindingLazyRegistry::registered;
asIScriptEngine *ASBindingLazyRegistry::bound_engine = nullptr;

// core 只要求 Object（AS 侧所有对象句柄的根）；其余常用基础类型交给扫描与兜底。
const char *const ASBindingLazyRegistry::CORE_TYPES[] = { "Object" };
const int ASBindingLazyRegistry::CORE_TYPE_COUNT = 1;

ASBindingLazyRegistry *ASBindingLazyRegistry::get_singleton() {
	static ASBindingLazyRegistry singleton;
	return &singleton;
}

bool ASBindingLazyRegistry::startup_mode_is_full() {
	GLOBAL_DEF("angel_script/binding/startup_mode", String("auto"));
	return String(GLOBAL_GET("angel_script/binding/startup_mode")) == "full";
}

bool ASBindingLazyRegistry::scan_project_enabled() {
	GLOBAL_DEF("angel_script/binding/scan_project", true);
	return bool(GLOBAL_GET("angel_script/binding/scan_project"));
}

void ASBindingLazyRegistry::reset() {
	full = false;
	skeleton_done = false;
	registered.clear();
	bound_engine = nullptr;
}

void ASBindingLazyRegistry::_bind_engine(asIScriptEngine *p_engine) {
	if (bound_engine == p_engine) {
		return;
	}
	full = false;
	skeleton_done = false;
	registered.clear();
	bound_engine = p_engine;
}

void ASBindingLazyRegistry::_register_type_set(const ASBindingPlan &p_plan, const Vector<StringName> &p_types, asIScriptEngine *p_engine) {
	Vector<StringName> to_add;
	for (const StringName &t : p_types) {
		if (full || registered.has(t)) {
			continue;
		}
		if (!p_plan.has_class(t)) {
			continue; // 不在可见集合（白名单排除/黑名单），静默忽略。
		}
		to_add.push_back(t);
	}
	if (to_add.is_empty()) {
		return;
	}
	// 返回值有意忽略：子集注册若失败，由编译重试与全量兜底收敛。
	ASBindingRegistry::register_types(p_plan, to_add, p_engine);
	for (const StringName &t : to_add) {
		registered.insert(t);
	}
}

void ASBindingLazyRegistry::ensure_initialized(const ASBindingPlan &p_plan, asIScriptEngine *p_engine, const Vector<String> &p_project_sources) {
	_bind_engine(p_engine);

	if (startup_mode_is_full()) {
		register_all(p_plan, p_engine);
		return;
	}
	if (!skeleton_done) {
		// 值类型 + 全部骨架 + 对象转换构造器（一次性，约 1047 类 × 3 次 Register*）。
		ASBindingRegistry::register_value_types_and_skeletons(p_plan, p_engine);
		skeleton_done = true;
		// @GlobalScope 工具函数只依赖值类型，可早注册，成本极低。
		ASBindingRegistry::register_globals(p_engine);
	}

	// core。
	Vector<StringName> core;
	for (int i = 0; i < CORE_TYPE_COUNT; i++) {
		core.push_back(StringName(CORE_TYPES[i]));
	}
	_register_type_set(p_plan, core, p_engine);

	// 阶段 1：扫描工程 .as 的类名并集。
	if (scan_project_enabled()) {
		const Vector<StringName> scanned = ASBindingScanner::scan_project(p_project_sources);
		_register_type_set(p_plan, scanned, p_engine);
	}
}

int64_t ASBindingLazyRegistry::ensure_registered(const ASBindingPlan &p_plan, const Vector<StringName> &p_types, asIScriptEngine *p_engine) {
	_bind_engine(p_engine);

	if (full) {
		return 0;
	}
	if (!skeleton_done) {
		return -1; // 未完成骨架阶段，调用方应走 ensure_initialized/兜底。
	}

	int64_t added = 0;
	Vector<StringName> to_add;
	for (const StringName &t : p_types) {
		if (registered.has(t)) {
			continue;
		}
		if (!p_plan.has_class(t)) {
			continue; // 不在可见集合（白名单排除/黑名单），静默忽略。
		}
		to_add.push_back(t);
		added++;
	}
	if (to_add.is_empty()) {
		return 0;
	}
	ASBindingRegistry::register_types(p_plan, to_add, p_engine);
	for (const StringName &t : to_add) {
		registered.insert(t);
	}
	return added;
}

void ASBindingLazyRegistry::register_all(const ASBindingPlan &p_plan, asIScriptEngine *p_engine) {
	_bind_engine(p_engine);

	if (full) {
		return;
	}
	ASBindingRegistry::register_plan(p_plan, p_engine);
	full = true;
	registered.clear();
}
