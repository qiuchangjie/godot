/**************************************************************************/
/*  as_binding_decl.cpp                                                   */
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

#include "as_binding_decl.h"

#include <angelscript.h>

String ASBindingDecl::variant_type_to_as(Variant::Type p_type) {
	switch (p_type) {
		// 标量：AS 原生类型。INT 必须是 64 位，Variant::INT 是 int64，32 位会截断。
		case Variant::BOOL: return "bool";
		case Variant::INT: return "int64";
		case Variant::FLOAT: return "double";
		// 内建值类型：本模块统一以 Variant 为存储，AS 侧按类型名注册。
		case Variant::STRING: return "String";
		case Variant::STRING_NAME: return "StringName";
		case Variant::NODE_PATH: return "NodePath";
		case Variant::RID: return "RID";
		case Variant::VECTOR2: return "Vector2";
		case Variant::VECTOR2I: return "Vector2i";
		case Variant::VECTOR3: return "Vector3";
		case Variant::VECTOR3I: return "Vector3i";
		case Variant::VECTOR4: return "Vector4";
		case Variant::VECTOR4I: return "Vector4i";
		case Variant::RECT2: return "Rect2";
		case Variant::RECT2I: return "Rect2i";
		case Variant::TRANSFORM2D: return "Transform2D";
		case Variant::PLANE: return "Plane";
		case Variant::QUATERNION: return "Quaternion";
		case Variant::AABB: return "AABB";
		case Variant::BASIS: return "Basis";
		case Variant::TRANSFORM3D: return "Transform3D";
		case Variant::PROJECTION: return "Projection";
		case Variant::COLOR: return "Color";
		case Variant::PACKED_BYTE_ARRAY: return "PackedByteArray";
		case Variant::PACKED_INT32_ARRAY: return "PackedInt32Array";
		case Variant::PACKED_INT64_ARRAY: return "PackedInt64Array";
		case Variant::PACKED_FLOAT32_ARRAY: return "PackedFloat32Array";
		case Variant::PACKED_FLOAT64_ARRAY: return "PackedFloat64Array";
		case Variant::PACKED_STRING_ARRAY: return "PackedStringArray";
		case Variant::PACKED_VECTOR2_ARRAY: return "PackedVector2Array";
		case Variant::PACKED_VECTOR3_ARRAY: return "PackedVector3Array";
		case Variant::PACKED_VECTOR4_ARRAY: return "PackedVector4Array";
		case Variant::PACKED_COLOR_ARRAY: return "PackedColorArray";
		case Variant::ARRAY: return "Array";
		case Variant::DICTIONARY: return "Dictionary";
		case Variant::CALLABLE: return "Callable";
		case Variant::SIGNAL: return "Signal";
		default: return String();
	}
}

ASBindingType ASBindingDecl::resolve(const PropertyInfo &p_info) {
	ASBindingType out;
	if (p_info.type == Variant::OBJECT) {
		out.valid = true;
		out.kind = AS_KIND_OBJECT;
		out.as_name = String(p_info.class_name.is_empty() ? StringName("Object") : p_info.class_name) + "@";
		return out;
	}
	String as = variant_type_to_as(p_info.type);
	if (as.is_empty()) {
		// 只写裸类型名，由调用方按场景拼 reason 前缀（return/param/property）。
		out.unbound_reason = String(Variant::get_type_name(p_info.type));
		return out;
	}
	out.valid = true;
	out.as_name = as;
	if (p_info.type == Variant::BOOL) {
		out.kind = AS_KIND_BOOL;
	} else if (p_info.type == Variant::INT) {
		out.kind = AS_KIND_INT64;
	} else if (p_info.type == Variant::FLOAT) {
		out.kind = AS_KIND_DOUBLE;
	} else {
		out.kind = AS_KIND_VALUE;
	}
	return out;
}

String as_binding_render_param(const String &p_as_name) {
	if (p_as_name == "bool" || p_as_name == "int64" || p_as_name == "double") {
		return p_as_name;
	}
	if (p_as_name.ends_with("@")) {
		return p_as_name;
	}
	return "const " + p_as_name + " &in";
}

static String render_param(const ASBindingType &t) {
	return as_binding_render_param(t.as_name);
}

bool ASBindingDecl::method_to_decl(const MethodInfo &p_info, String *r_decl, String *r_reason) {
	if (r_decl) {
		*r_decl = String();
	}
	if (r_reason) {
		*r_reason = String();
	}

	// vararg 无法在 AS 静态类型里表达（AS 只允许 asCALL_GENERIC 的 vararg 函数）。
	if (p_info.flags & METHOD_FLAG_VARARG) {
		if (r_reason) {
			*r_reason = "vararg";
		}
		return false;
	}

	// AS 2.38 的解析器不识别 `static` 关键字：把 "static ..." 交给 RegisterObjectMethod
	// 会返回 asINVALID_DECLARATION 并永久污染引擎（探针验证）。M2 一律不绑定静态方法。
	if (p_info.flags & METHOD_FLAG_STATIC) {
		if (r_reason) {
			*r_reason = "static-method";
		}
		return false;
	}

	String ret;
	if (p_info.return_val.type == Variant::NIL) {
		// 真正返回 Variant 的方法（property_info 带 NIL_IS_VARIANT）在 M2 没有可编组的
		// 返回类型：声明成 void 会静默丢弃返回值（半注册），必须整体归为 unbound。
		if (p_info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
			if (r_reason) {
				*r_reason = "unsupported-return-type: Variant";
			}
			return false;
		}
		ret = "void";
	} else {
		ASBindingType rt = resolve(p_info.return_val);
		if (!rt.valid) {
			if (r_reason) {
				*r_reason = "unsupported-return-type: " + rt.unbound_reason;
			}
			return false;
		}
		ret = rt.as_name;
	}

	String args;
	for (int i = 0; i < p_info.arguments.size(); i++) {
		ASBindingType at = resolve(p_info.arguments[i]);
		if (!at.valid) {
			if (r_reason) {
				*r_reason = vformat("unsupported-param-type: %s (arg %d)", at.unbound_reason, i);
			}
			return false;
		}
		if (i > 0) {
			args += ", ";
		}
		args += render_param(at);
	}

	String decl;
	decl += ret + " " + p_info.name + "(" + args + ")";
	if (p_info.flags & METHOD_FLAG_CONST) {
		decl += " const";
	}

	if (r_decl) {
		*r_decl = decl;
	}
	return true;
}

Variant as_binding_marshal_arg(asIScriptGeneric *p_gen, int p_index, ASBindingKind p_kind) {
	switch (p_kind) {
		case AS_KIND_BOOL:
			return Variant((bool)p_gen->GetArgByte(p_index));
		case AS_KIND_INT64:
			return Variant((int64_t)p_gen->GetArgQWord(p_index));
		case AS_KIND_DOUBLE:
			return Variant(p_gen->GetArgDouble(p_index));
		case AS_KIND_VALUE: {
			// const T &in：GetArgObject 返回对象地址，其中存的是本模块统一的 Variant 存储。
			void *p = p_gen->GetArgObject(p_index);
			return p ? *(const Variant *)p : Variant();
		}
		case AS_KIND_OBJECT:
			return Variant((Object *)p_gen->GetArgObject(p_index));
		default:
			return Variant();
	}
}

void as_binding_marshal_return(asIScriptGeneric *p_gen, ASBindingKind p_kind, const Variant &p_value) {
	switch (p_kind) {
		case AS_KIND_BOOL:
			*(bool *)p_gen->GetAddressOfReturnLocation() = (bool)p_value;
			break;
		case AS_KIND_INT64:
			*(int64_t *)p_gen->GetAddressOfReturnLocation() = (int64_t)p_value;
			break;
		case AS_KIND_DOUBLE:
			*(double *)p_gen->GetAddressOfReturnLocation() = (double)p_value;
			break;
		case AS_KIND_VALUE:
			// 值类型按值返回：交给 AS 用注册的拷贝构造写进返回槽。
			p_gen->SetReturnObject((void *)&p_value);
			break;
		case AS_KIND_OBJECT:
			p_gen->SetReturnObject((void *)p_value.operator Object *());
			break;
		default:
			break;
	}
}
