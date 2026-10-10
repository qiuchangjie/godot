/**************************************************************************/
/*  as_binding_plan.h                                                     */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/pair.h"
#include "core/variant/variant.h"

#include "as_binding_decl.h"

// 绑定计划：ClassDB/Variant 内省的纯数据中间表示。
// 运行期注册器与 .d.as dumper 消费同一份 plan，一致性由代码结构保证（spec §3.8.2 方案 A）。
struct ASBindingMethod {
	String name;
	String as_decl;
	const MethodBind *bind = nullptr;
	bool is_static = false;
	// 跳板按 AS_KIND_* 决定如何把实参编组为 Variant；由 ASBindingDecl::resolve 产出，与 as_decl 同源。
	Vector<ASBindingKind> param_kinds;
	ASBindingKind return_kind = AS_KIND_VOID;
};

struct ASBindingProperty {
	String name;
	String as_type;
	// 供 as_binding_object.cpp 直接使用，避免再从 as_type 字符串反推 marshal kind。
	ASBindingKind kind = AS_KIND_VOID;
	bool read_only = false;
};

struct ASBindingConstant {
	String name;
	int64_t value = 0;
};

struct ASBindingClass {
	StringName name;
	StringName parent;
	bool instantiable = false;
	Vector<ASBindingMethod> methods;
	Vector<ASBindingProperty> properties;
	Vector<ASBindingConstant> constants;
};

struct ASBindingValueType {
	String as_name;
	Variant::Type type = Variant::NIL;
};

struct ASBindingEnum {
	StringName scope;
	String name;
	Vector<Pair<String, int64_t>> values;
};

struct ASUnboundEntry {
	StringName owner;
	String member;
	String reason;
};

struct ASBindingScope {
	PackedStringArray whitelist;
	PackedStringArray blacklist;
	static ASBindingScope from_project_settings();
};

class ASBindingPlan {
	Vector<ASBindingClass> classes; // 拓扑序，Object 最先。
	Vector<ASBindingValueType> value_types;
	Vector<ASBindingEnum> enums;
	Vector<ASUnboundEntry> unbound;

	void _build_class(const StringName &p_class, const HashSet<StringName> &p_visible);

public:
	void build(const ASBindingScope &p_scope);

	const Vector<ASBindingClass> &get_classes() const { return classes; }
	const Vector<ASBindingValueType> &get_value_types() const { return value_types; }
	const Vector<ASBindingEnum> &get_enums() const { return enums; }
	const Vector<ASUnboundEntry> &get_unbound() const { return unbound; }

	// 稳定序列化：用于 .d.as 版本 hash 与 dump 确定性测试（Task 5）。
	String serialize_stable() const;

	// 类型是否在可见集合内（供惰性注册过滤黑名单/白名单）。
	bool has_class(const StringName &p_name) const;
};
