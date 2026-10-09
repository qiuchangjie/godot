/**************************************************************************/
/*  as_script_language.cpp                                                */
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

#include "as_script_language.h"

#include "as_engine.h"
#include "as_script.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

ASScriptLanguage *ASScriptLanguage::singleton = nullptr;

ASScriptLanguage::ASScriptLanguage() {
	singleton = this;
}

ASScriptLanguage::~ASScriptLanguage() {
	singleton = nullptr;
}

void ASScriptLanguage::init() {
	// ASEngine 自身也是懒初始化的，这里只是让引擎在语言被正式启用时提前就绪
	// （--test / headless 路径不会走这里，靠 ASEngine::get_singleton() 兜底）。
	ASEngine::get_singleton()->ensure_initialized();

	// M3：宿主节流回收的间隔（秒）。0 表示关闭宿主侧兜底，只保留 AS 自身的回收。
	GLOBAL_DEF_BASIC("angel_script/gc/interval_seconds", 5.0);
	ASEngine::get_singleton()->set_gc_interval_seconds(GLOBAL_GET("angel_script/gc/interval_seconds"));
}

void ASScriptLanguage::frame() {
	ASEngine *as_engine = ASEngine::get_singleton();
	if (as_engine != nullptr) {
		as_engine->maybe_collect_garbage();
	}

#ifdef DEBUG_ENABLED
	if (profiling) {
		MutexLock lock(profile_mutex);
		// 把本帧计数滚动为「上一帧」并清零，供 profiling_get_frame_data() 读取；
		// 累计数据（call_count/total_time）保持不动。frame 与采样写入同锁，避免撕裂。
		for (KeyValue<StringName, ProfileEntry> &kv : profile_data) {
			kv.value.last_frame_call_count = kv.value.frame_call_count;
			kv.value.last_frame_total_time = kv.value.frame_total_time;
			kv.value.frame_call_count = 0;
			kv.value.frame_total_time = 0;
		}
	}
#endif
}

void ASScriptLanguage::finish() {
	ASEngine *as_engine = ASEngine::get_singleton();
	if (as_engine != nullptr) {
		as_engine->collect_garbage(); // 退出前最后收一次，减少退出期泄漏告警
	}
	ASEngine::get_singleton()->shutdown();
}

Vector<String> ASScriptLanguage::get_reserved_words() const {
	static const char *words[] = {
		"and", "abstract", "auto", "bool", "break", "case", "cast", "catch", "class",
		"const", "continue", "default", "do", "double", "else", "enum", "explicit",
		"external", "false",
		"final", "float", "for", "from", "funcdef", "get", "if", "import", "in", "inout",
		"int", "int8", "int16", "int32", "int64", "interface", "is", "mixin",
		"namespace", "not", "null", "or", "out", "override", "private", "protected",
		"return", "set", "shared", "string", "switch", "true", "try", "typedef", "uint",
		"uint8", "uint16", "uint32", "uint64", "void", "while", "xor", nullptr
	};
	Vector<String> out;
	for (int i = 0; words[i] != nullptr; i++) {
		out.push_back(words[i]);
	}
	return out;
}

bool ASScriptLanguage::is_control_flow_keyword(const String &p_string) const {
	return p_string == "break" || p_string == "case" || p_string == "continue" ||
			p_string == "default" || p_string == "do" || p_string == "else" ||
			p_string == "for" || p_string == "if" || p_string == "return" ||
			p_string == "switch" || p_string == "while";
}

Vector<String> ASScriptLanguage::get_comment_delimiters() const {
	Vector<String> out;
	out.push_back("//");
	out.push_back("/* */");
	return out;
}

Vector<String> ASScriptLanguage::get_doc_comment_delimiters() const {
	return Vector<String>();
}

Vector<String> ASScriptLanguage::get_string_delimiters() const {
	Vector<String> out;
	out.push_back("\" \"");
	out.push_back("' '");
	return out;
}

bool ASScriptLanguage::validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const {
	return true;
}

int ASScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	return -1;
}

String ASScriptLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
	return String();
}

Ref<Script> ASScriptLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<ASScript> scr;
	scr.instantiate();

	// 本语言未开启编辑器模板系统（is_using_templates() 为 false，对话框传入的 p_template
	// 恒为空），因此这里按模块约定合成最小可用骨架：`// godot_base:` 基类指令 +
	// 与文件名同名的空脚本类。缺了这一步，基类返回的空引用会让
	// ScriptCreateDialog::_create_new() 在 scr->set_path() 处空指针崩溃。
	String base = p_base_class_name.strip_edges().unquote();
	if (base.is_empty() || !ClassDB::class_exists(base)) {
		base = "Object";
	}

	scr->set_source_code(vformat("// godot_base: %s\n\nclass %s {\n}\n", base, p_class_name.strip_edges()));
	return scr;
}

String ASScriptLanguage::debug_get_error() const {
	return String();
}

int ASScriptLanguage::debug_get_stack_level_count() const {
	return 0;
}

int ASScriptLanguage::debug_get_stack_level_line(int p_level) const {
	return -1;
}

String ASScriptLanguage::debug_get_stack_level_function(int p_level) const {
	return String();
}

String ASScriptLanguage::debug_get_stack_level_source(int p_level) const {
	return String();
}

void ASScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void ASScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void ASScriptLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

String ASScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	return String();
}

void ASScriptLanguage::reload_all_scripts() {
}

void ASScriptLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
#ifdef TOOLS_ENABLED
	for (int i = 0; i < p_scripts.size(); i++) {
		Object *obj = p_scripts[i];
		Ref<ASScript> scr = Object::cast_to<ASScript>(obj);
		if (scr.is_null()) {
			continue;
		}
		// 与 GDScript 不同，soft/hard 都必须摘除实例：AS 实例持有模块内的
		// asIScriptObject，重编译会 DiscardModule，不先摘除即悬垂。
		scr->reload_with_instances(p_soft_reload);
	}
#endif
}

void ASScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	Array scripts;
	scripts.push_back(p_script);
	reload_scripts(scripts, p_soft_reload);
}

void ASScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(get_extension());
	p_extensions->push_back("asb");
}

void ASScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
}

void ASScriptLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
}

void ASScriptLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
}

void ASScriptLanguage::profiling_start() {
#ifdef DEBUG_ENABLED
	MutexLock lock(profile_mutex);
	profile_data.clear();
	profiling = true;
#endif
}

void ASScriptLanguage::profiling_stop() {
#ifdef DEBUG_ENABLED
	MutexLock lock(profile_mutex);
	profiling = false;
#endif
}

bool ASScriptLanguage::is_profiling() const {
#ifdef DEBUG_ENABLED
	return profiling;
#else
	return false;
#endif
}

void ASScriptLanguage::profile_function(const StringName &p_signature, uint64_t p_usec) {
#ifdef DEBUG_ENABLED
	MutexLock lock(profile_mutex);
	// 采样已停止时不得再累加：call_function 路径已用 is_profiling() 预筛，
	// 但直接调用（含测试）仍需此处兜底，否则会污染停止后的历史数据。
	if (!profiling) {
		return;
	}
	ProfileEntry &entry = profile_data[p_signature];
	entry.call_count++;
	entry.total_time += p_usec;
	entry.frame_call_count++;
	entry.frame_total_time += p_usec;
#endif
}

int ASScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
	int current = 0;
#ifdef DEBUG_ENABLED
	MutexLock lock(profile_mutex);
	for (const KeyValue<StringName, ProfileEntry> &kv : profile_data) {
		if (current >= p_info_max) {
			break;
		}
		ProfilingInfo &info = p_info_arr[current];
		info.signature = kv.key;
		info.call_count = kv.value.call_count;
		info.total_time = kv.value.total_time;
		// v1 不拆分嵌套 AS→AS 调用，自身耗时即总耗时。
		info.self_time = kv.value.total_time;
		// v1 不统计 AS→Godot 原生调用。
		info.internal_time = 0;
		current++;
	}
#endif
	return current;
}

int ASScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
	int current = 0;
#ifdef DEBUG_ENABLED
	MutexLock lock(profile_mutex);
	for (const KeyValue<StringName, ProfileEntry> &kv : profile_data) {
		if (current >= p_info_max) {
			break;
		}
		if (kv.value.last_frame_call_count == 0) {
			continue;
		}
		ProfilingInfo &info = p_info_arr[current];
		info.signature = kv.key;
		info.call_count = kv.value.last_frame_call_count;
		info.total_time = kv.value.last_frame_total_time;
		info.self_time = kv.value.last_frame_total_time;
		info.internal_time = 0;
		current++;
	}
#endif
	return current;
}
