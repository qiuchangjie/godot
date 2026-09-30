/**************************************************************************/
/*  as_script_instance.h                                                  */
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

#include "as_script.h"

#include "core/object/script_instance.h"

#include <angelscript.h>

// 脚本实例：持有 AS 侧对象句柄（object）与一个专用执行上下文（context）。
// 引擎回调 _ready/_enter_tree/_exit_tree/_process/_physics_process 由 Node 的
// GDVIRTUAL 经 has_method()/callp() 按名字派发（scene/main/node.h:433-437），
// notification() 只负责 _notification(int)，与 gdscript 的处理一致。
class ASScriptInstance : public ScriptInstance {
	Object *owner = nullptr;
	Ref<ASScript> script;
	asIScriptObject *object = nullptr;
	asIScriptContext *context = nullptr;

	int _find_property(const StringName &p_name, int *r_type_id = nullptr) const;
	static int _expected_param_count(const StringName &p_method);

public:
	ASScriptInstance(const Ref<ASScript> &p_script, Object *p_owner);
	virtual ~ASScriptInstance() override;

	virtual bool set(const StringName &p_name, const Variant &p_value) override;
	virtual bool get(const StringName &p_name, Variant &r_ret) const override;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const override;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;
	virtual void validate_property(PropertyInfo &p_property) const override {}
	virtual bool property_can_revert(const StringName &p_name) const override { return false; }
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override { return false; }
	virtual Object *get_owner() override { return owner; }
	virtual void get_method_list(List<MethodInfo> *p_list) const override;
	virtual bool has_method(const StringName &p_method) const override;
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	virtual void notification(int p_notification, bool p_reversed = false) override;
	virtual Ref<Script> get_script() const override { return script; }
	virtual ScriptLanguage *get_language() override;
};
