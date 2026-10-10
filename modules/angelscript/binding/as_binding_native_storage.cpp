/**************************************************************************/
/*  as_binding_native_storage.cpp                                         */
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

#include "as_binding_native_storage.h"

#include "as_binding_native_value_ops.h"

#include "core/error/error_macros.h"
#include "core/math/vector2.h"

#include <angelscript.h>
#include <new>

#if AS_NATIVE_VALUE_OPS_ENABLED

namespace {

// 原生默认构造：AS 传入的 self 是 sizeof(Vector2) 的原始内存。
// 必须显式写入 (0,0)：非 POD 类型若不注册默认构造，AS 在 `Vector2 v;` 处无法创建对象。
void ctor_default(Vector2 *self) {
	new (self) Vector2();
}

// 原生 (double,double) 构造。AS 侧 FLOAT 映射为 double（见 as_binding_decl.cpp variant_type_to_as）。
void ctor_xy(Vector2 *self, double p_x, double p_y) {
	new (self) Vector2((real_t)p_x, (real_t)p_y);
}

// 原生拷贝构造。非 POD 值类型必须有拷贝构造/赋值/析构（as_scriptengine.cpp:3418-3427），
// 且这些行为让 COMPLEX_RETURN_MASK != 0，从而 AS 用内存返回小结构体（见下 op_add）。
void ctor_copy(Vector2 *self, const Vector2 *p_other) {
	new (self) Vector2(*p_other);
}

void dtor(Vector2 *self) {
	self->~Vector2();
}

Vector2 *op_assign(Vector2 *self, const Vector2 *p_other) {
	*self = *p_other;
	return self;
}

// x/y 属性访问器。
double get_x(const Vector2 *self) {
	return (double)self->x;
}

void set_x(Vector2 *self, double p_v) {
	self->x = (real_t)p_v;
}

double get_y(const Vector2 *self) {
	return (double)self->y;
}

void set_y(Vector2 *self, double p_v) {
	self->y = (real_t)p_v;
}

// 二元运算符。Vector2 是 8 字节、仅含 float 的小结构体：MSVC x64 会把它放在 XMM0
// 返回，而 AngelScript（无 SPLIT_OBJS_BY_MEMBER_TYPES / ALLFLOATS 路径，
// as_config.h:509-516、as_callfunc.cpp:323-326）在 hostReturnFloat=false 时只读 RAX，
// 二者 ABI 不一致。故强制内存返回：AS 传入隐藏的 ret 指针（as_callfunc_x64_msvc.cpp:77-91，
// CDECL_OBJFIRST 的顺序是 ret, self, ...），thunk 用 placement new 就地构造返回值。
void op_add(Vector2 *ret, const Vector2 *self, const Vector2 *p_other) {
	ASNativeValueOps::note_thunk_call();
	new (ret) Vector2(*self + *p_other);
}

} // namespace

namespace ASNativeValueStorage {

bool register_vector2_skeleton(asIScriptEngine *p_engine) {
	if (p_engine->GetTypeInfoByName("Vector2") != nullptr) {
		return true; // 幂等：已注册。
	}
	// 非 POD：显式注册构造/析构/赋值，并带 APP_CLASS sub-flag 让
	// COMPLEX_RETURN_MASK != 0（as_config.h:515），从而小结构体走内存返回，
	// 绕开 MSVC x64 上 AS 无法正确读回 float 结构体寄存器返回的缺陷。
	const asDWORD flags = asOBJ_VALUE | asOBJ_APP_CLASS |
			asOBJ_APP_CLASS_CONSTRUCTOR | asOBJ_APP_CLASS_DESTRUCTOR |
			asOBJ_APP_CLASS_ASSIGNMENT | asOBJ_APP_CLASS_COPY_CONSTRUCTOR;
	if (p_engine->RegisterObjectType("Vector2", sizeof(Vector2), flags) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 skeleton.");
		return false;
	}
	return true;
}

bool register_vector2_members(asIScriptEngine *p_engine) {
	asIScriptEngine *e = p_engine;

	if (e->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ctor_default), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 default constructor.");
		return false;
	}
	if (e->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f(double, double)", asFUNCTION(ctor_xy), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 (double,double) constructor.");
		return false;
	}
	if (e->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f(const Vector2 &in)", asFUNCTION(ctor_copy), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 copy constructor.");
		return false;
	}
	if (e->RegisterObjectBehaviour("Vector2", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(dtor), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 destructor.");
		return false;
	}
	if (e->RegisterObjectMethod("Vector2", "Vector2 &opAssign(const Vector2 &in)", asFUNCTION(op_assign), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT("AngelScript: failed to register native Vector2 opAssign.");
		return false;
	}
	e->RegisterObjectMethod("Vector2", "double get_x() const property", asFUNCTION(get_x), asCALL_CDECL_OBJFIRST);
	e->RegisterObjectMethod("Vector2", "void set_x(double) property", asFUNCTION(set_x), asCALL_CDECL_OBJFIRST);
	e->RegisterObjectMethod("Vector2", "double get_y() const property", asFUNCTION(get_y), asCALL_CDECL_OBJFIRST);
	e->RegisterObjectMethod("Vector2", "void set_y(double) property", asFUNCTION(set_y), asCALL_CDECL_OBJFIRST);
	e->RegisterObjectMethod("Vector2", "Vector2 opAdd(const Vector2 &in) const", asFUNCTION(op_add), asCALL_CDECL_OBJFIRST);
	return true;
}

} // namespace ASNativeValueStorage

#else // !AS_NATIVE_VALUE_OPS_ENABLED

namespace ASNativeValueStorage {

bool register_vector2_skeleton(asIScriptEngine *p_engine) {
	(void)p_engine;
	return false; // 未验证平台：整体回退 generic。
}

bool register_vector2_members(asIScriptEngine *p_engine) {
	(void)p_engine;
	return false;
}

} // namespace ASNativeValueStorage

#endif // AS_NATIVE_VALUE_OPS_ENABLED
