/**************************************************************************/
/*  as_script_instance.cpp                                                */
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

#include "as_script_instance.h"

#include "as_engine.h"
#include "as_script.h"
#include "as_script_language.h"

#include "core/object/object.h"

ASScriptInstance::ASScriptInstance(const Ref<ASScript> &p_script, Object *p_owner) {
	script = p_script;
	owner = p_owner;

	asITypeInfo *type = script->get_type_info();
	ERR_FAIL_NULL_MSG(type, "AngelScript type info is missing for the script instance.");

	asIScriptEngine *engine = ASEngine::get_singleton()->get_engine();
	ERR_FAIL_NULL_MSG(engine, "AngelScript engine is not initialized.");

	context = engine->CreateContext();
	ERR_FAIL_NULL_MSG(context, "Failed to create an AngelScript context.");

	// 默认构造（无参 factory）必须在此完成：之后引擎只按名字调方法、按索引读写属性。
	asIScriptFunction *factory = type->GetFactoryByIndex(0);
	if (factory == nullptr) {
		// 具名局部量承载 utf8 缓冲：临时 CharString 的 data() 不能跨语句使用（Task 3 同法）。
		CharString default_decl = (String(type->GetName()) + "@ " + String(type->GetName()) + "()").utf8();
		factory = type->GetFactoryByDecl(default_decl.get_data());
	}
	ERR_FAIL_NULL_MSG(factory, "AngelScript class has no default factory (default constructor).");

	if (context->Prepare(factory) < 0) {
		ERR_FAIL_MSG("Failed to prepare the AngelScript factory.");
	}
	if (context->Execute() != asEXECUTION_FINISHED) {
		context->Unprepare();
		ERR_FAIL_MSG("AngelScript constructor execution failed.");
	}

	object = *(asIScriptObject **)context->GetAddressOfReturnValue();
	if (object != nullptr) {
		// 先自持一份再 Unprepare：避免上下文释放返回值时把对象一起带走。
		object->AddRef();
	}
	context->Unprepare();
}

ASScriptInstance::~ASScriptInstance() {
	if (object != nullptr) {
		object->Release();
		object = nullptr;
	}
	if (context != nullptr) {
		context->Release();
		context = nullptr;
	}
}

int ASScriptInstance::_find_property(const StringName &p_name, int *r_type_id) const {
	asITypeInfo *type = script->get_type_info();
	if (type == nullptr) {
		return -1;
	}
	for (asUINT i = 0; i < type->GetPropertyCount(); i++) {
		// asITypeInfo 用 GetProperty(index, &name, &type_id) 枚举属性；
		// GetPropertyName/GetPropertyTypeId 是 asIScriptObject（实例接口）的成员。
		const char *prop_name = nullptr;
		int type_id = 0;
		if (type->GetProperty(i, &prop_name, &type_id) < 0 || prop_name == nullptr) {
			continue;
		}
		if (String(p_name) == String(prop_name)) {
			if (r_type_id != nullptr) {
				*r_type_id = type_id;
			}
			return (int)i;
		}
	}
	return -1;
}

int ASScriptInstance::_expected_param_count(const StringName &p_method) {
	// 引擎只按固定签名派发这几个 GDVIRTUAL 回调（scene/main/node.h:433-437）；
	// 签名不匹配就当方法不存在，否则 call() 会因参数个数校验失败而报错。
	if (p_method == "_ready" || p_method == "_enter_tree" || p_method == "_exit_tree") {
		return 0;
	}
	if (p_method == "_process" || p_method == "_physics_process" || p_method == "_notification") {
		return 1;
	}
	return -1;
}

bool ASScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	int type_id = 0;
	const int index = _find_property(p_name, &type_id);
	if (index < 0 || object == nullptr) {
		return false;
	}
	void *addr = object->GetAddressOfProperty((asUINT)index);
	if (addr == nullptr) {
		return false;
	}
	switch (type_id & ~asTYPEID_OBJHANDLE) {
		case asTYPEID_BOOL:
			*(bool *)addr = p_value.operator bool();
			return true;
		case asTYPEID_INT32:
			*(int32_t *)addr = (int32_t)p_value.operator int64_t();
			return true;
		case asTYPEID_INT64:
			*(int64_t *)addr = p_value.operator int64_t();
			return true;
		case asTYPEID_FLOAT:
			*(float *)addr = (float)p_value.operator double();
			return true;
		case asTYPEID_DOUBLE:
			*(double *)addr = p_value.operator double();
			return true;
		default:
			// 对象/字符串等引用型属性留待绑定层（M2）。
			return false;
	}
}

bool ASScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	int type_id = 0;
	const int index = _find_property(p_name, &type_id);
	if (index < 0 || object == nullptr) {
		return false;
	}
	const void *addr = object->GetAddressOfProperty((asUINT)index);
	if (addr == nullptr) {
		return false;
	}
	// AS 的内存布局：bool 1 字节、int 4 字节、float 4 字节；这里按各自宽度读，避免越界。
	switch (type_id & ~asTYPEID_OBJHANDLE) {
		case asTYPEID_BOOL:
			r_ret = Variant(*(const bool *)addr);
			return true;
		case asTYPEID_INT32:
			r_ret = Variant((int64_t)*(const int32_t *)addr);
			return true;
		case asTYPEID_INT64:
			r_ret = Variant((int64_t)*(const int64_t *)addr);
			return true;
		case asTYPEID_FLOAT:
			r_ret = Variant((double)*(const float *)addr);
			return true;
		case asTYPEID_DOUBLE:
			r_ret = Variant(*(const double *)addr);
			return true;
		default:
			return false;
	}
}

void ASScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	script->get_script_property_list(p_properties);
}

Variant::Type ASScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	int type_id = 0;
	if (_find_property(p_name, &type_id) < 0) {
		if (r_is_valid != nullptr) {
			*r_is_valid = false;
		}
		return Variant::NIL;
	}
	if (r_is_valid != nullptr) {
		*r_is_valid = true;
	}
	switch (type_id & ~asTYPEID_OBJHANDLE) {
		case asTYPEID_BOOL:
			return Variant::BOOL;
		case asTYPEID_INT32:
		case asTYPEID_INT64:
			return Variant::INT;
		case asTYPEID_FLOAT:
		case asTYPEID_DOUBLE:
			return Variant::FLOAT;
		default:
			return Variant::NIL;
	}
}

void ASScriptInstance::get_method_list(List<MethodInfo> *p_list) const {
	script->get_script_method_list(p_list);
}

bool ASScriptInstance::has_method(const StringName &p_method) const {
	asITypeInfo *type = script->get_type_info();
	if (type == nullptr) {
		return false;
	}
	CharString name = String(p_method).utf8();
	asIScriptFunction *func = type->GetMethodByName(name.get_data(), true);
	if (func == nullptr) {
		return false;
	}
	const int expected = _expected_param_count(p_method);
	if (expected >= 0 && (int)func->GetParamCount() != expected) {
		return false;
	}
	return true;
}

Variant ASScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;

	asITypeInfo *type = script->get_type_info();
	if (type == nullptr) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}
	CharString name = String(p_method).utf8();
	asIScriptFunction *func = type->GetMethodByName(name.get_data(), true);
	if (func == nullptr) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}
	const int expected = _expected_param_count(p_method);
	if (expected >= 0 && (int)func->GetParamCount() != expected) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	Variant ret;
	const Error err = ASEngine::call_function(context, func, object, p_args, p_argcount, &ret);
	if (err == ERR_INVALID_PARAMETER) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		return Variant();
	}
	if (err != OK) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}
	return ret;
}

void ASScriptInstance::notification(int p_notification, bool p_reversed) {
	// 只处理 _notification(int)；_ready/_process 等由引擎经 has_method/callp 按名字调用。
	Variant arg = p_notification;
	const Variant *args[1] = { &arg };
	Callable::CallError error;
	callp("_notification", args, 1, error);
}

ScriptLanguage *ASScriptInstance::get_language() {
	return ASScriptLanguage::get_singleton();
}
