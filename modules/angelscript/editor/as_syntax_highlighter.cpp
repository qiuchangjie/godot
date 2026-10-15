/**************************************************************************/
/*  as_syntax_highlighter.cpp                                             */
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

#include "as_syntax_highlighter.h"

#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/text_edit.h"

void ASSyntaxHighlighter::_bind_methods() {}

Color ASSyntaxHighlighter::_token_color(ASTokenType p_type) const {
	switch (p_type) {
		case ASTokenType::KEYWORD:
			return keyword_color;
		case ASTokenType::BUILTIN_TYPE:
			return base_type_color;
		case ASTokenType::ENGINE_TYPE:
			return engine_type_color;
		case ASTokenType::USER_TYPE:
			return user_type_color;
		case ASTokenType::FUNCTION:
			return function_color;
		case ASTokenType::MEMBER:
			return member_variable_color;
		case ASTokenType::STRING:
			return string_color;
		case ASTokenType::NUMBER:
			return number_color;
		case ASTokenType::COMMENT:
		case ASTokenType::DIRECTIVE:
			return comment_color;
		case ASTokenType::SYMBOL:
			return symbol_color;
		case ASTokenType::IDENTIFIER:
		default:
			return font_color;
	}
}

void ASSyntaxHighlighter::_update_cache() {
	// 主题或类型集变化时整体失效，避免用到旧颜色/旧类型集。
	block_comment_state.clear();

	if (text_edit == nullptr) {
		return;
	}

	font_color = text_edit->get_theme_color(SceneStringName(font_color));
	symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");
	keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	base_type_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
	engine_type_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	user_type_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");
	comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	number_color = EDITOR_GET("text_editor/theme/highlighting/number_color");
	function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	member_variable_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");

	// 引擎类型集合：只在编辑器内省一次，供词法器判定 ENGINE_TYPE。
	engine_types.clear();
	LocalVector<StringName> class_list;
	ClassDB::get_class_list(class_list);
	for (const StringName &type : class_list) {
		if (ClassDB::is_class_exposed(type)) {
			engine_types.insert(type);
		}
	}

	// 用户全局类（GDScript/AngelScript 注册进 ScriptServer 的类名）。
	user_types.clear();
	LocalVector<StringName> global_classes;
	ScriptServer::get_global_class_list(global_classes);
	for (const StringName &type : global_classes) {
		user_types.insert(type);
	}
}

void ASSyntaxHighlighter::_clear_highlighting_cache() {
	// 基类在文本编辑后会失效单行缓存，但不一定调用本函数；这里清块注释状态，
	// 且 `_get_line_syntax_highlighting_impl` 的走查会在缺项时按需重算，保证不串色。
	block_comment_state.clear();
}

Dictionary ASSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;
	if (text_edit == nullptr) {
		return color_map;
	}

	// 求本行行首的块注释状态：与 CodeHighlighter 一致，从最近的已知行向前推进。
	bool in_block_comment = false;
	if (p_line != 0) {
		int prev_region_line = p_line - 1;
		while (prev_region_line > 0 && !block_comment_state.has(prev_region_line)) {
			prev_region_line--;
		}
		for (int i = prev_region_line; i < p_line - 1; i++) {
			get_line_syntax_highlighting(i);
		}
		if (!block_comment_state.has(p_line - 1)) {
			get_line_syntax_highlighting(p_line - 1);
		}
		if (block_comment_state.has(p_line - 1)) {
			in_block_comment = block_comment_state[p_line - 1];
		}
	}

	const String line = text_edit->get_line_with_ime(p_line);
	const int line_length = line.length();

	bool out_block_comment = in_block_comment;
	const Vector<ASToken> tokens = ASHighlighterLexer::tokenize(line, in_block_comment, engine_types, user_types, out_block_comment);
	// 先记录行尾状态，即使本行是空行也要落到缓存，供后续行走查。
	block_comment_state[p_line] = out_block_comment;

	if (line_length == 0) {
		return color_map;
	}

	// 先整行填默认前景色，再用 token 覆盖；最后压缩成「只在颜色变化列写点」，
	// 这样 token 之间的空白会正确回落到默认色。
	Vector<Color> colors;
	colors.resize(line_length);
	for (int i = 0; i < line_length; i++) {
		colors.write[i] = font_color;
	}
	for (const ASToken &token : tokens) {
		const Color color = _token_color(token.type);
		const int end = MIN(token.end, line_length);
		for (int i = MAX(token.start, 0); i < end; i++) {
			colors.write[i] = color;
		}
	}

	Color prev_color = colors[0];
	{
		Dictionary info;
		info["color"] = prev_color;
		color_map[0] = info;
	}
	for (int i = 1; i < line_length; i++) {
		if (colors[i] != prev_color) {
			prev_color = colors[i];
			Dictionary info;
			info["color"] = prev_color;
			color_map[i] = info;
		}
	}
	return color_map;
}

Ref<EditorSyntaxHighlighter> ASSyntaxHighlighter::_create() const {
	Ref<ASSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	return highlighter;
}

String ASSyntaxHighlighter::_get_name() const {
	return "AngelScript";
}

PackedStringArray ASSyntaxHighlighter::_get_supported_languages() const {
	return PackedStringArray{ "AngelScript" };
}
