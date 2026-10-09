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

// 符号判定不能直接用 char_utils 的 is_symbol（它把空格/制表也算作 symbol），
// 故此处排除标识符字符、空白与字符串引号（引号已在字符串分支消费）。
static bool _is_as_symbol(char32_t p_char) {
	return !is_ascii_identifier_char(p_char) && !is_whitespace(p_char) && p_char != '"';
}

Vector<ASToken> ASHighlighterLexer::tokenize(const String &p_line, bool p_in_block_comment, const HashSet<StringName> &p_engine_types, const HashSet<StringName> &p_user_types, bool &r_out_block_comment) {
	Vector<ASToken> tokens;
	const int length = p_line.length();
	int i = 0;
	bool in_block = p_in_block_comment;

	while (i < length) {
		const char32_t c = p_line[i];

		// 1) 块注释续行。
		if (in_block) {
			const int close = p_line.find("*/", i);
			if (close == -1) {
				tokens.push_back({ i, length, ASTokenType::COMMENT });
				i = length;
			} else {
				tokens.push_back({ i, close + 2, ASTokenType::COMMENT });
				i = close + 2;
				in_block = false;
			}
			continue;
		}

		// 2) 行注释与 `// godot_base:` 指令。
		if (c == '/' && i + 1 < length && p_line[i + 1] == '/') {
			const String body = p_line.substr(i + 2).strip_edges();
			tokens.push_back({ i, length, body.begins_with("godot_base:") ? ASTokenType::DIRECTIVE : ASTokenType::COMMENT });
			i = length;
			continue;
		}

		// 3) 块注释起始（单行或延续到后续行）。
		if (c == '/' && i + 1 < length && p_line[i + 1] == '*') {
			const int close = p_line.find("*/", i + 2);
			if (close == -1) {
				tokens.push_back({ i, length, ASTokenType::COMMENT });
				in_block = true;
				i = length;
			} else {
				tokens.push_back({ i, close + 2, ASTokenType::COMMENT });
				i = close + 2;
			}
			continue;
		}

		// 4) 字符串（不跨行，\" 转义）。
		if (c == '"') {
			int j = i + 1;
			while (j < length) {
				if (p_line[j] == '\\' && j + 1 < length) {
					j += 2;
					continue;
				}
				if (p_line[j] == '"') {
					j++;
					break;
				}
				j++;
			}
			tokens.push_back({ i, j, ASTokenType::STRING });
			i = j;
			continue;
		}

		// 5) 数字。
		if (is_digit(c)) {
			int j = i;
			if (c == '0' && i + 1 < length && (p_line[i + 1] == 'x' || p_line[i + 1] == 'X')) {
				j = i + 2;
				while (j < length && is_hex_digit(p_line[j])) {
					j++;
				}
			} else if (c == '0' && i + 1 < length && (p_line[i + 1] == 'b' || p_line[i + 1] == 'B')) {
				j = i + 2;
				while (j < length && is_binary_digit(p_line[j])) {
					j++;
				}
			} else {
				while (j < length && is_digit(p_line[j])) {
					j++;
				}
				if (j < length && p_line[j] == '.') {
					j++;
					while (j < length && is_digit(p_line[j])) {
						j++;
					}
				}
				if (j < length && (p_line[j] == 'e' || p_line[j] == 'E')) {
					int k = j + 1;
					if (k < length && (p_line[k] == '+' || p_line[k] == '-')) {
						k++;
					}
					if (k < length && is_digit(p_line[k])) {
						j = k;
						while (j < length && is_digit(p_line[j])) {
							j++;
						}
					}
				}
				if (j < length && (p_line[j] == 'f' || p_line[j] == 'F' || p_line[j] == 'd' || p_line[j] == 'D' || p_line[j] == 'u' || p_line[j] == 'U')) {
					j++;
				}
			}
			tokens.push_back({ i, j, ASTokenType::NUMBER });
			i = j;
			continue;
		}

		// 6) 标识符 / 关键字 / 基本类型 / 引擎类型 / 用户类型 / 函数 / 成员。
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
			} else if (p_engine_types.has(word)) {
				type = ASTokenType::ENGINE_TYPE;
			} else if (p_user_types.has(word)) {
				type = ASTokenType::USER_TYPE;
			} else if (i > 0 && p_line[i - 1] == '.') {
				// 前接 '.' → 成员访问；放在类型判定之后，保证类型名优先于成员判定。
				type = ASTokenType::MEMBER;
			} else {
				// 后随（可跨空白）'(' → 函数调用。
				int k = j;
				while (k < length && (p_line[k] == ' ' || p_line[k] == '\t')) {
					k++;
				}
				if (k < length && p_line[k] == '(') {
					type = ASTokenType::FUNCTION;
				}
			}
			tokens.push_back({ i, j, type });
			i = j;
			continue;
		}

		// 7) 符号。
		if (_is_as_symbol(c)) {
			tokens.push_back({ i, i + 1, ASTokenType::SYMBOL });
			i++;
			continue;
		}

		i++;
	}

	r_out_block_comment = in_block;
	return tokens;
}
