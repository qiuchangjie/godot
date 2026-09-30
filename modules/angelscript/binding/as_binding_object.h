#pragma once

// 把 ClassDB 里可见的对象类型注册进 AngelScript。
//
// 注册分两遍（必须遵守，见 spec §3.8 M2 规格）：
//   1. register_skeleton()：只建类型骨架（RegisterObjectType + addref/release + 可选 factory），
//      让后续任何签名引用都能解析到类型名。
//   2. register_class()：挂方法/属性/常量/opImplCast。
// 原因是 AS 2.38 里任何一次 Register* 失败都会给引擎置上 configFailed，
// 之后所有编译都会报 "Invalid configuration"，不可恢复。
//
// 对象在 AS 侧的存储就是 Object*（sizeof(void*)），行为由 generic trampoline 转发到 MethodBind / Object::get|set。

#include "as_binding_plan.h"
#include "core/error/error_list.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#include <angelscript.h>

class ASBindingObject {
public:
	// 第一遍：建类型骨架。可重复调用（已存在的类型直接跳过）。
	static Error register_skeleton(const ASBindingClass &p_class, asIScriptEngine *p_engine);
	// 第二遍：挂成员（方法/属性/常量/向上转换）。同一类型只挂一次。
	static Error register_class(const ASBindingClass &p_class, asIScriptEngine *p_engine);
	// 注册 ClassDB 枚举。枚举名里的 '.' 会被替换成 '_'（AS 标识符不允许点号），
	// 即脚本侧写作 `Node_ProcessMode::PROCESS_MODE_ALWAYS`。
	static Error register_enums(const Vector<ASBindingEnum> &p_enums, asIScriptEngine *p_engine);

	// 以下为 asCALL_GENERIC 跳板，函数指针交给 Register*。
	static void generic_method_call(asIScriptGeneric *p_gen);
	static void generic_upcast(asIScriptGeneric *p_gen);
	static void generic_instantiate(asIScriptGeneric *p_gen);
	// RefCounted 语义：句柄持有即加引用。
	static void generic_addref(asIScriptGeneric *p_gen);
	static void generic_release(asIScriptGeneric *p_gen);
	// 非 RefCounted（Node 等）：AS 句柄只是借用，且对象随时可能被 free()。
	// 这两个跳板绝不能解引用指针，否则会在已释放的内存上调虚函数。
	static void generic_addref_noop(asIScriptGeneric *p_gen);
	static void generic_release_noop(asIScriptGeneric *p_gen);
	static void generic_property_get(asIScriptGeneric *p_gen);
	static void generic_property_set(asIScriptGeneric *p_gen);
};
