/**************************************************************************/
/*  as_binding_registry.cpp                                               */
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

#include "as_binding_registry.h"

#include "as_binding_object.h"
#include "as_binding_value_types.h"

Error ASBindingRegistry::register_plan(const ASBindingPlan &p_plan, asIScriptEngine *p_engine) {
	ERR_FAIL_NULL_V(p_engine, ERR_INVALID_PARAMETER);

	const Error err = ASBindingValueTypes::register_all(p_engine);
	if (err != OK) {
		return err;
	}

	// 两遍注册：先给所有类建骨架，再挂成员。方法签名会引用其他类，
	// 一次性逐类注册会让先注册的类因“未知类型”失败，而 AS 里任何一次
	// Register* 失败都会把引擎标记为配置错误且不可恢复。
	for (const ASBindingClass &c : p_plan.get_classes()) {
		ASBindingObject::register_skeleton(c, p_engine);
	}
	for (const ASBindingClass &c : p_plan.get_classes()) {
		ASBindingObject::register_class(c, p_engine);
	}

	ASBindingObject::register_enums(p_plan.get_enums(), p_engine);
	return OK;
}
