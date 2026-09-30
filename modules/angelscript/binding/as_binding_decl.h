/**************************************************************************/
/*  as_binding_decl.h                                                     */
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

#include "core/object/object.h"
#include "core/string/string_name.h"
#include "core/variant/variant.h"

// 跳板在运行期按形参/返回值的“类别”决定如何编组：标量走 AS 原生类型，
// 内建值类型走 Variant 存储（本模块的统一内存布局），对象走 Object*。
enum ASBindingKind {
	AS_KIND_VOID,
	AS_KIND_BOOL,
	AS_KIND_INT64,
	AS_KIND_DOUBLE,
	AS_KIND_VALUE,
	AS_KIND_OBJECT,
};

struct ASBindingType {
	bool valid = false;
	String as_name; // "int64" / "String" / "Node@" / "Vector2"
	ASBindingKind kind = AS_KIND_VOID;
	String unbound_reason; // 仅在 valid == false 时非空。
};

class ASBindingDecl {
public:
	// Variant 标量/值类型 -> AS 类型名（OBJECT 不在此处理，因为它需要 class_name）。
	static String variant_type_to_as(Variant::Type p_type);
	// PropertyInfo -> AS 类型（含 OBJECT 的 class_name；枚举型属性退化为 int64）。
	static ASBindingType resolve(const PropertyInfo &p_info);
	// MethodInfo -> AS 声明串（不含所属类名）。失败时返回 false 并写 r_reason。
	static bool method_to_decl(const MethodInfo &p_info, String *r_decl, String *r_reason);
};
