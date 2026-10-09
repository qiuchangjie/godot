/**************************************************************************/
/*  test_angelscript_highlighter.cpp                                      */
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

#include "editor/as_highlighter_lexer.h"

#ifdef TOOLS_ENABLED

#define ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL
#include "test_angelscript_highlighter.h"

namespace {

Vector<ASToken> tokenize_default(const String &p_line, bool p_in_block = false, bool *r_out = nullptr, const HashSet<StringName> &p_engine = HashSet<StringName>(), const HashSet<StringName> &p_user = HashSet<StringName>()) {
	bool out = false;
	Vector<ASToken> tokens = ASHighlighterLexer::tokenize(p_line, p_in_block, p_engine, p_user, out);
	if (r_out != nullptr) {
		*r_out = out;
	}
	return tokens;
}

// 返回整行中第一个指定类别的 token 文本；没有则返回空串。
String first_token_text(const String &p_line, ASTokenType p_type) {
	for (const ASToken &token : tokenize_default(p_line)) {
		if (token.type == p_type) {
			return p_line.substr(token.start, token.end - token.start);
		}
	}
	return String();
}

int count_tokens(const String &p_line, ASTokenType p_type) {
	int count = 0;
	for (const ASToken &token : tokenize_default(p_line)) {
		if (token.type == p_type) {
			count++;
		}
	}
	return count;
}

} // namespace

void as_highlighter_lexer_recognizes_keywords_and_builtin_types() {
	CHECK(first_token_text("if (a) { return; }", ASTokenType::KEYWORD) == "if");
	CHECK(count_tokens("if (a) { return; }", ASTokenType::KEYWORD) == 2); // if, return
	CHECK(first_token_text("class Foo {}", ASTokenType::KEYWORD) == "class");
	CHECK(first_token_text("while (x) {}", ASTokenType::KEYWORD) == "while");

	CHECK(first_token_text("int x = 0;", ASTokenType::BUILTIN_TYPE) == "int");
	CHECK(first_token_text("auto y = 1;", ASTokenType::BUILTIN_TYPE) == "auto");
	CHECK(first_token_text("string s;", ASTokenType::BUILTIN_TYPE) == "string");
	CHECK(first_token_text("uint64 big;", ASTokenType::BUILTIN_TYPE) == "uint64");

	// 大小写敏感：Int 不是基本类型。
	CHECK(first_token_text("Int value;", ASTokenType::BUILTIN_TYPE).is_empty());

	// 普通标识符。
	CHECK(first_token_text("foo bar;", ASTokenType::IDENTIFIER) == "foo");
}

#endif // TOOLS_ENABLED
