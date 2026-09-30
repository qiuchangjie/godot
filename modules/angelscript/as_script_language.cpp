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
}

void ASScriptLanguage::finish() {
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
}

void ASScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
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
}

void ASScriptLanguage::profiling_stop() {
}

int ASScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}

int ASScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}
