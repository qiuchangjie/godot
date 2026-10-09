/**************************************************************************/
/*  as_syntax_highlighter.h                                               */
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

#include "as_highlighter_lexer.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/script/syntax_highlighters.h"

// 把 ASHighlighterLexer 的 token 映射成编辑器主题色。
// 跨行块注释状态用与 GDScriptSyntaxHighlighter/CodeHighlighter 相同的「按行缓存 + 向前走查」策略，
// 而不是计划里设想的 block_comment_cached_to：走查法已被上游验证，且不需要额外字段。
class ASSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(ASSyntaxHighlighter, EditorSyntaxHighlighter);

	Color symbol_color;
	Color keyword_color;
	Color base_type_color;
	Color engine_type_color;
	Color user_type_color;
	Color comment_color;
	Color string_color;
	Color number_color;
	Color function_color;
	Color member_variable_color;
	Color font_color;

	HashSet<StringName> engine_types;
	HashSet<StringName> user_types;

	// 行号 → 该行「行尾」是否处于块注释内（等价于下一行的行首状态）。
	HashMap<int, bool> block_comment_state;

	Color _token_color(ASTokenType p_type) const;

protected:
	static void _bind_methods();

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
	virtual void _clear_highlighting_cache() override;
	virtual Ref<EditorSyntaxHighlighter> _create() const override;
	virtual String _get_name() const override;
	virtual PackedStringArray _get_supported_languages() const override;
};
