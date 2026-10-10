/**************************************************************************/
/*  as_script.h                                                           */
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

#include "core/object/script_language.h"
#include "core/templates/hash_set.h"

#include <angelscript.h>

class ASScriptInstance;

// 一个 .as 资源 = 一个 AngelScript 模块 + 其中与文件名同名的脚本类。
// 本阶段（M1）刻意不实现实例化：instance_create() 返回 nullptr，由 Task 4 的
// ASScriptInstance 接管。基类由源码前 10 行内的 `// godot_base: <ClassDB 类型>` 声明（D2）。
class ASScript : public Script {
	GDSOFTCLASS(ASScript, Script);

	String source_code;
	StringName class_name;
	StringName instance_base_type;
	String module_name;
	String compile_error;
	bool valid = false;

	// 由 `signal_<name>` 约定方法收集来的信号声明（见 _collect_signals）。
	// 参数只支持可映射到 Variant 的类型；含对象参数或不支持类型的信号整体忽略。
	struct ASSignalInfo {
		StringName name;
		Vector<PropertyInfo> args;
	};
	Vector<ASSignalInfo> signals;
	void _collect_signals();

	static asITypeInfo *_find_script_class(asIScriptModule *p_module, const String &p_class_name);

#ifdef TOOLS_ENABLED
	// 编辑器在脚本尚不可实例化时（如刚创建、未编译）会请求占位实例（见 placeholder_instance_create）。
	// 集合用于在占位实例析构时经 _placeholder_erased 回收，避免留下悬垂指针。
	HashSet<PlaceHolderScriptInstance *> placeholders;

	// 已实例化的脚本实例集合。热重载必须在重编译前摘除它们：reload→clear 会
	// DiscardModule，仍被引用的 asIScriptObject 会随之悬垂。集合让语言层能枚举承载对象。
	HashSet<ASScriptInstance *> instances;
#endif

public:
	static String get_class_name_for_path(const String &p_path);

	ASScript();
	virtual ~ASScript() override;

	bool compile_source(const String &p_source, const String &p_path, String *r_error);
	// 将已编译模块序列化为 `.asb` 容器并写入 p_out_path。
	// p_required_types 为加载前需要预注册的 ClassDB 类型集合（由编译工具扫描得到）。
	Error save_bytecode(const String &p_out_path, const Vector<StringName> &p_required_types, String *r_error = nullptr);
	// 从 `.asb` 容器字节加载脚本：先按容器符号表增量注册 ClassDB 类型，再反序列化模块。
	// 基类由模块类型信息推导；失败时清空自身，不留下半成品脚本。
	bool load_bytecode(const Vector<uint8_t> &p_bytes, const String &p_path, String *r_error = nullptr);
	asIScriptModule *get_module() const;
	asITypeInfo *get_type_info() const;
	void clear();

	virtual bool can_instantiate() const override { return valid; }
	virtual Ref<Script> get_base_script() const override { return Ref<Script>(); }
	virtual StringName get_global_name() const override { return StringName(); }
	virtual bool inherits_script(const Ref<Script> &p_script) const override { return false; }
	virtual StringName get_instance_base_type() const override { return instance_base_type; }
	virtual ScriptInstance *instance_create(Object *p_this) override;
	virtual PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
#ifdef TOOLS_ENABLED
	void _add_instance(ASScriptInstance *p_instance);
	void _remove_instance(ASScriptInstance *p_instance);
	// 重编译后刷新所有占位实例的属性列表，触发 Inspector 重绘（等价 GDScript update_exports）。
	void update_placeholders();
	// 热重载：摘除实例与占位实例、重编译、重挂并恢复属性状态。
	// 编辑器保存 .as 时经 ASScriptLanguage::reload_tool_script 调用。
	void reload_with_instances(bool p_keep_state);
#endif
	virtual bool has_source_code() const override { return !source_code.is_empty(); }
	virtual String get_source_code() const override { return source_code; }
	virtual void set_source_code(const String &p_code) override { source_code = p_code; }
	virtual Error reload(bool p_keep_state = false) override;
#ifdef TOOLS_ENABLED
	virtual StringName get_doc_class_name() const override { return StringName(); }
	virtual Vector<DocData::ClassDoc> get_documentation() const override { return Vector<DocData::ClassDoc>(); }
	virtual String get_class_icon_path() const override { return String(); }
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;
#endif
	virtual bool has_method(const StringName &p_method) const override;
	virtual MethodInfo get_method_info(const StringName &p_method) const override { return MethodInfo(); }
	virtual bool is_tool() const override { return false; }
	virtual bool is_script_valid() const override { return valid; }
	virtual bool is_abstract() const override { return false; }
	virtual ScriptLanguage *get_language() const override;
	virtual bool has_script_signal(const StringName &p_signal) const override;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override;
	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override { return false; }
	virtual void get_script_method_list(List<MethodInfo> *p_list) const override;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const override;
	virtual const Variant get_rpc_config() const override { return Variant(); }
};
