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

// 仅为取得 AS_NATIVE_VALUE_OPS_ENABLED 门控宏（定义于该头文件）。它不是“未使用包含”，
// 删掉会让下方 #if 静默退化为 #else（原生存储整体失效）。
#include "as_binding_native_value_ops.h"
#include "core/error/error_macros.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"

#include <angelscript.h>
#include <new>

#if AS_NATIVE_VALUE_OPS_ENABLED

namespace {

// 4 个原生存储类型的公共生命周期 thunk。它们都是平凡可拷贝/析构的小结构体，
// 无需类型专属逻辑，用模板统一生成原生（asCALL_CDECL_OBJFIRST）行为，
// 避免热路径上每个临时对象析构都走 asCALL_GENERIC 跳板（见 Task6 性能验证）。
template <typename T>
void native_ctor_default(T *self) {
	new (self) T();
}

template <typename T>
void native_ctor_copy(T *self, const T *p_other) {
	new (self) T(*p_other);
}

template <typename T>
void native_dtor(T *self) {
	self->~T();
}

} // namespace

namespace ASNativeValueStorage {

bool is_native_storage_type(Variant::Type p_type) {
	return p_type == Variant::VECTOR2 || p_type == Variant::VECTOR3 ||
			p_type == Variant::VECTOR2I || p_type == Variant::VECTOR3I;
}

bool register_native_lifecycle(asIScriptEngine *p_engine, const String &p_name, Variant::Type p_type) {
	void *fn_ctor_default = nullptr;
	void *fn_ctor_copy = nullptr;
	void *fn_dtor = nullptr;
	switch (p_type) {
		case Variant::VECTOR2:
			fn_ctor_default = (void *)&native_ctor_default<Vector2>;
			fn_ctor_copy = (void *)&native_ctor_copy<Vector2>;
			fn_dtor = (void *)&native_dtor<Vector2>;
			break;
		case Variant::VECTOR3:
			fn_ctor_default = (void *)&native_ctor_default<Vector3>;
			fn_ctor_copy = (void *)&native_ctor_copy<Vector3>;
			fn_dtor = (void *)&native_dtor<Vector3>;
			break;
		case Variant::VECTOR2I:
			fn_ctor_default = (void *)&native_ctor_default<Vector2i>;
			fn_ctor_copy = (void *)&native_ctor_copy<Vector2i>;
			fn_dtor = (void *)&native_dtor<Vector2i>;
			break;
		case Variant::VECTOR3I:
			fn_ctor_default = (void *)&native_ctor_default<Vector3i>;
			fn_ctor_copy = (void *)&native_ctor_copy<Vector3i>;
			fn_dtor = (void *)&native_dtor<Vector3i>;
			break;
		default:
			return false; // 非原生类型：调用方走 generic。
	}
	CharString cname = p_name.utf8();
	const CharString copy_decl = ("void f(const " + p_name + " &in)").utf8();
	if (p_engine->RegisterObjectBehaviour(cname.get_data(), asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(fn_ctor_default), asCALL_CDECL_OBJFIRST) < 0 ||
			p_engine->RegisterObjectBehaviour(cname.get_data(), asBEHAVE_CONSTRUCT, copy_decl.get_data(), asFUNCTION(fn_ctor_copy), asCALL_CDECL_OBJFIRST) < 0 ||
			p_engine->RegisterObjectBehaviour(cname.get_data(), asBEHAVE_DESTRUCT, "void f()", asFUNCTION(fn_dtor), asCALL_CDECL_OBJFIRST) < 0) {
		ERR_PRINT(vformat("AngelScript: failed to register native lifecycle for '%s'.", p_name));
		return false;
	}
	return true;
}

bool register_skeleton(asIScriptEngine *p_engine, Variant::Type p_type) {
	const char *name = nullptr;
	int size = 0;
	switch (p_type) {
		case Variant::VECTOR2: name = "Vector2"; size = sizeof(Vector2); break;
		case Variant::VECTOR3: name = "Vector3"; size = sizeof(Vector3); break;
		case Variant::VECTOR2I: name = "Vector2i"; size = sizeof(Vector2i); break;
		case Variant::VECTOR3I: name = "Vector3i"; size = sizeof(Vector3i); break;
		default: return false;
	}
	if (p_engine->GetTypeInfoByName(name) != nullptr) {
		return true; // 幂等
	}
	const asDWORD flags = asOBJ_VALUE | asOBJ_APP_CLASS | asOBJ_APP_CLASS_CONSTRUCTOR |
			asOBJ_APP_CLASS_DESTRUCTOR | asOBJ_APP_CLASS_ASSIGNMENT | asOBJ_APP_CLASS_COPY_CONSTRUCTOR;
	if (p_engine->RegisterObjectType(name, size, flags) < 0) {
		ERR_PRINT(vformat("AngelScript: failed to register native value type '%s'.", name));
		return false;
	}
	return true;
}

Variant native_to_variant(Variant::Type p_type, const void *p_slot) {
	switch (p_type) {
		case Variant::VECTOR2: return *(const Vector2 *)p_slot;
		case Variant::VECTOR3: return *(const Vector3 *)p_slot;
		case Variant::VECTOR2I: return *(const Vector2i *)p_slot;
		case Variant::VECTOR3I: return *(const Vector3i *)p_slot;
		default: return Variant();
	}
}

void variant_to_native(Variant::Type p_type, const Variant &p_value, void *p_slot) {
	switch (p_type) {
		case Variant::VECTOR2: *(Vector2 *)p_slot = (Vector2)p_value; break;
		case Variant::VECTOR3: *(Vector3 *)p_slot = (Vector3)p_value; break;
		case Variant::VECTOR2I: *(Vector2i *)p_slot = (Vector2i)p_value; break;
		case Variant::VECTOR3I: *(Vector3i *)p_slot = (Vector3i)p_value; break;
		default: break;
	}
}

bool write_native_return(asIScriptGeneric *p_gen, Variant::Type p_type, const Variant &p_value) {
	switch (p_type) {
		case Variant::VECTOR2: { Vector2 t = (Vector2)p_value; p_gen->SetReturnObject(&t); return true; }
		case Variant::VECTOR3: { Vector3 t = (Vector3)p_value; p_gen->SetReturnObject(&t); return true; }
		case Variant::VECTOR2I: { Vector2i t = (Vector2i)p_value; p_gen->SetReturnObject(&t); return true; }
		case Variant::VECTOR3I: { Vector3i t = (Vector3i)p_value; p_gen->SetReturnObject(&t); return true; }
		default: return false;
	}
}

} // namespace ASNativeValueStorage

#else // !AS_NATIVE_VALUE_OPS_ENABLED

namespace ASNativeValueStorage {

bool is_native_storage_type(Variant::Type p_type) {
	(void)p_type;
	return false;
}

bool register_native_lifecycle(asIScriptEngine *p_engine, const String &p_name, Variant::Type p_type) {
	(void)p_engine;
	(void)p_name;
	(void)p_type;
	return false;
}

bool register_skeleton(asIScriptEngine *p_engine, Variant::Type p_type) {
	(void)p_engine;
	(void)p_type;
	return false;
}

Variant native_to_variant(Variant::Type p_type, const void *p_slot) {
	(void)p_type;
	(void)p_slot;
	return Variant();
}

void variant_to_native(Variant::Type p_type, const Variant &p_value, void *p_slot) {
	(void)p_type;
	(void)p_value;
	(void)p_slot;
}

bool write_native_return(asIScriptGeneric *p_gen, Variant::Type p_type, const Variant &p_value) {
	(void)p_gen;
	(void)p_type;
	(void)p_value;
	return false;
}

} // namespace ASNativeValueStorage

#endif // AS_NATIVE_VALUE_OPS_ENABLED
