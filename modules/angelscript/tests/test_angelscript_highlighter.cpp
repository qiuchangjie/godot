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

#include "editor/as_syntax_highlighter.h"

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

void as_highlighter_lexer_recognizes_literals_comments_and_directives() {
	// 字符串：含转义。
	CHECK(first_token_text("\"hello\" + x", ASTokenType::STRING) == "\"hello\"");
	CHECK(first_token_text("\"a\\\"b\"", ASTokenType::STRING) == "\"a\\\"b\"");
	// 未闭合字符串必须在行尾结束，且不产生 COMMENT。
	CHECK(first_token_text("\"abc", ASTokenType::STRING) == "\"abc");
	CHECK(count_tokens("\"abc", ASTokenType::COMMENT) == 0);
	// 字符串里的 // 与 /* 不是注释。
	CHECK(count_tokens("\"a//b\"", ASTokenType::COMMENT) == 0);
	CHECK(count_tokens("\"a/*b\"", ASTokenType::COMMENT) == 0);
	CHECK(first_token_text("\"a//b\"", ASTokenType::STRING) == "\"a//b\"");

	// 数字：十六进制 / 二进制 / 浮点 / 指数 / 后缀。
	CHECK(first_token_text("0x1F", ASTokenType::NUMBER) == "0x1F");
	CHECK(first_token_text("0b1010", ASTokenType::NUMBER) == "0b1010");
	CHECK(first_token_text("3.14", ASTokenType::NUMBER) == "3.14");
	CHECK(first_token_text("1e10", ASTokenType::NUMBER) == "1e10");
	CHECK(first_token_text("2.0f", ASTokenType::NUMBER) == "2.0f");
	CHECK(first_token_text("10u", ASTokenType::NUMBER) == "10u");

	// 符号。
	CHECK(first_token_text("f(x)", ASTokenType::SYMBOL) == "(");

	// 行注释。
	CHECK(first_token_text("x // trailing", ASTokenType::COMMENT) == "// trailing");

	// `// godot_base:` 指令（容忍 // 后无空格）。
	CHECK(first_token_text("// godot_base: Node", ASTokenType::DIRECTIVE) == "// godot_base: Node");
	CHECK(first_token_text("//godot_base:Node", ASTokenType::DIRECTIVE) == "//godot_base:Node");
	CHECK(count_tokens("// graceful comment", ASTokenType::DIRECTIVE) == 0);

	// 单行块注释。
	CHECK(first_token_text("a /* b */ c", ASTokenType::COMMENT) == "/* b */");

	// 多行块注释：开行 out=true，闭行 out=false 且其后恢复正常着色。
	bool out = false;
	Vector<ASToken> open_tokens = tokenize_default("/* start", false, &out);
	CHECK(out == true);
	CHECK(open_tokens.size() == 1);
	CHECK(open_tokens[0].type == ASTokenType::COMMENT);

	Vector<ASToken> close_tokens = tokenize_default(" end */ x", true, &out);
	CHECK(out == false);
	// 注意：first_token_text/count_tokens 辅助函数按 p_in_block=false 分词，无法表达续行；
	// 这里直接断言 block-aware 的 close_tokens。
	CHECK(close_tokens.size() == 2);
	CHECK(close_tokens[0].type == ASTokenType::COMMENT);
	CHECK(close_tokens[0].end == 7); // " end */"
	CHECK(close_tokens[1].type == ASTokenType::IDENTIFIER); // x

	// 已在块注释内的整行（无闭合）仍为注释且 out 保持 true。
	Vector<ASToken> mid_tokens = tokenize_default("still inside", true, &out);
	CHECK(out == true);
	CHECK(mid_tokens.size() == 1);
	CHECK(mid_tokens[0].type == ASTokenType::COMMENT);

	// 空行 / 纯空白不产出 token。
	CHECK(tokenize_default("").is_empty());
	CHECK(tokenize_default("   ").is_empty());
}

void as_highlighter_lexer_classifies_identifiers_with_type_precedence() {
	HashSet<StringName> engine_types;
	engine_types.insert("Node");
	engine_types.insert("Vector2");

	HashSet<StringName> user_types;
	user_types.insert("MyThing");

	bool out = false;

	// 引擎类型 / 用户类型。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("Node n;", false, engine_types, user_types, out);
		CHECK(tokens.size() == 3);
		CHECK(tokens[0].type == ASTokenType::ENGINE_TYPE);
		CHECK(tokens[1].type == ASTokenType::IDENTIFIER);
	}
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("MyThing t;", false, engine_types, user_types, out);
		CHECK(tokens[0].type == ASTokenType::USER_TYPE);
	}

	// 函数调用：标识符后随 '('。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("print(1);", false, engine_types, user_types, out);
		CHECK(tokens[0].type == ASTokenType::FUNCTION);
	}

	// 成员：前接 '.'。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("obj.hp", false, engine_types, user_types, out);
		bool found_member = false;
		for (const ASToken &token : tokens) {
			if (token.type == ASTokenType::MEMBER && token.start == 4) {
				found_member = true;
			}
		}
		CHECK(found_member);
	}

	// 函数优先于成员：obj.method() 里的方法是函数调用（与 spec / GDScript 观感一致）。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("obj.method()", false, engine_types, user_types, out);
		bool found_function = false;
		for (const ASToken &token : tokens) {
			if (token.type == ASTokenType::FUNCTION && token.start == 4) {
				found_function = true;
			}
		}
		CHECK(found_function);
	}

	// 类型判定优先于函数：Vector2(...) 是类型而非函数；int(...) 是基本类型。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("Vector2(1, 2)", false, engine_types, user_types, out);
		CHECK(tokens[0].type == ASTokenType::ENGINE_TYPE);
	}
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("int(x)", false, engine_types, user_types, out);
		CHECK(tokens[0].type == ASTokenType::BUILTIN_TYPE);
	}

	// 关键字优先于函数：if(...) 仍是关键字。
	{
		Vector<ASToken> tokens = ASHighlighterLexer::tokenize("if (x)", false, engine_types, user_types, out);
		CHECK(tokens[0].type == ASTokenType::KEYWORD);
	}
}

void as_syntax_highlighter_reports_language_and_creates_instances() {
	Ref<ASSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	REQUIRE(highlighter.is_valid());

	CHECK(highlighter->_get_name() == "AngelScript");
	CHECK(highlighter->_get_supported_languages().has("AngelScript"));

	Ref<EditorSyntaxHighlighter> created = highlighter->_create();
	CHECK(created.is_valid());
}

#endif // TOOLS_ENABLED
