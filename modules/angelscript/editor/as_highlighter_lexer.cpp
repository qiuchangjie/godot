/**************************************************************************/
/*  as_highlighter_lexer.cpp                                              */
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

#include "as_highlighter_lexer.h"

#include "core/string/char_utils.h"

// 关键字表不含基本类型（两者互斥，判定顺序见 tokenize）。
static const HashSet<String> as_keywords = {
	"abstract", "and", "break", "case", "cast", "class", "const", "continue",
	"default", "do", "else", "enum", "false", "final", "for", "from", "funcdef",
	"get", "if", "import", "in", "inout", "interface", "is", "mixin", "namespace",
	"not", "null", "or", "out", "override", "private", "protected", "return",
	"set", "shared", "switch", "this", "true", "typedef", "while", "xor"
};

// auto 归入基本类型（AngelScript 的 auto 是类型占位符）。
static const HashSet<String> as_builtin_types = {
	"auto", "bool", "int", "int8", "int16", "int32", "int64",
	"uint", "uint8", "uint16", "uint32", "uint64",
	"float", "double", "string", "void"
};

const HashSet<String> &ASHighlighterLexer::get_keywords() {
	return as_keywords;
}

const HashSet<String> &ASHighlighterLexer::get_builtin_types() {
	return as_builtin_types;
}

static bool _is_ident_start(char32_t p_char) {
	return is_ascii_alphabet_char(p_char) || p_char == '_';
}

static bool _is_ident_continue(char32_t p_char) {
	return is_ascii_identifier_char(p_char);
}

Vector<ASToken> ASHighlighterLexer::tokenize(const String &p_line, bool p_in_block_comment, const HashSet<StringName> &p_engine_types, const HashSet<StringName> &p_user_types, bool &r_out_block_comment) {
	Vector<ASToken> tokens;
	const int length = p_line.length();
	int i = 0;

	while (i < length) {
		const char32_t c = p_line[i];

		if (_is_ident_start(c)) {
			int j = i + 1;
			while (j < length && _is_ident_continue(p_line[j])) {
				j++;
			}
			const String word = p_line.substr(i, j - i);
			ASTokenType type = ASTokenType::IDENTIFIER;
			if (as_keywords.has(word)) {
				type = ASTokenType::KEYWORD;
			} else if (as_builtin_types.has(word)) {
				type = ASTokenType::BUILTIN_TYPE;
			}
			tokens.push_back({ i, j, type });
			i = j;
			continue;
		}

		// 字符串 / 注释 / 数字 / 符号 / 类型与函数判定在后续任务中补齐。
		(void)p_in_block_comment;
		(void)p_engine_types;
		(void)p_user_types;
		i++;
	}

	// 块注释状态在 Task 3 处理；此处先保持中立。
	r_out_block_comment = false;
	return tokens;
}
