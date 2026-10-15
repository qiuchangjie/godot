/**************************************************************************/
/*  as_highlighter_lexer.h                                                */
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

#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

// 词法类别。DIRECTIVE 指 `// godot_base:` 这类具备语义的注释，单独成类以便将来区分着色。
enum class ASTokenType {
	KEYWORD,
	BUILTIN_TYPE,
	ENGINE_TYPE,
	USER_TYPE,
	FUNCTION,
	MEMBER,
	STRING,
	NUMBER,
	COMMENT,
	DIRECTIVE,
	SYMBOL,
	IDENTIFIER,
};

// 一段被识别的源码区间，列号为 [start, end)（以 UTF-32 码点计，与 String::operator[] 一致）。
struct ASToken {
	int start = 0;
	int end = 0;
	ASTokenType type = ASTokenType::IDENTIFIER;
};

// 纯词法器：无全局状态、不依赖引擎，方便单测。高亮器负责跨行状态与颜色映射。
class ASHighlighterLexer {
public:
	static const HashSet<String> &get_keywords();
	static const HashSet<String> &get_builtin_types();

	// p_in_block_comment：本行行首是否处于块注释内。
	// r_out_block_comment：本行行尾是否处于块注释内（供下一行作为 p_in_block_comment）。
	static Vector<ASToken> tokenize(const String &p_line, bool p_in_block_comment, const HashSet<StringName> &p_engine_types, const HashSet<StringName> &p_user_types, bool &r_out_block_comment);
};
