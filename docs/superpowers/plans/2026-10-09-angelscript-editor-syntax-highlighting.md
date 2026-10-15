# AngelScript Editor Syntax Highlighting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Godot 编辑器里为 `.as` 脚本提供 token 级语法高亮（关键字 / 类型 / 字符串 / 数字 / 注释 / `// godot_base:` 指令 / 函数 / 成员 / 符号），改写即生效，颜色跟随编辑器主题。

**Architecture:** 一个不依赖引擎/编辑器的纯词法器 `ASHighlighterLexer` 把「单行源码 + 行首是否在块注释内」切成 `ASToken` 列表；一个 `ASSyntaxHighlighter : EditorSyntaxHighlighter` 读主题色、用与 `CodeHighlighter`/`GDScriptSyntaxHighlighter` 相同的 `color_region_cache` 走查法维护跨行块注释状态、把 token 映射成逐列颜色交给 `TextEdit`。词法器与高亮器分离，因此跨行状态逻辑可单独测，颜色/主题逻辑不进单测（与 GDScript 现状一致）。

**Tech Stack:** C++17、Godot 4.7 module 系统（SCons）、doctest 测试框架、AngelScript 2.38.0（**不新增**第三方依赖，词典自研）。

**Spec:** `docs/superpowers/specs/2026-10-09-angelscript-editor-syntax-highlighting-design.md`

## Global Constraints

- 新文件仅在编辑器构建（`env.editor_build` / `TOOLS_ENABLED`）下编译；运行时（`target=template_*`）零影响。
- 不新增第三方依赖；词法器自研。
- 构建命令（先 `vcvars64`）：`scons platform=windows target=editor tests=yes accesskit=no d3d12=no -j8`。仓库已封装的等价脚本：`.superpowers\sdd\2026-09-30-angelscript-hot-update-phase1\build-tests.cmd`。
- 测试命令（**必须用通配符**）：`bin\godot.windows.editor.x86_64.console.exe --headless --test --test-case="*AngelScript*"`。注意管道到 `Select-String` 可能显示卡住，建议 `cmd /c "... > %TEMP%\as-test.log 2>&1"` 后读文件。
- 命名风格遵循 Godot 官方：`snake_case`（函数/变量）、`PascalCase`（类型）、成员前缀 `_` 仅用于私有方法。
- 缩进用 Tab；新建 `.h/.cpp` 顶部沿用 Godot MIT 许可证注释块（从同目录既有文件复制）。
- 注释写「为什么」，用中文。
- **提交一律通过 `git-commit` 技能**（遵循仓库根 `AGENTS.md`，禁止手写 `git commit -m`）。下面的 Commit 步骤给出建议的 conventional 消息，实际用技能提交。

## Review Focus

以下是 spec 暗示、但任何单测都不会自动覆盖、最容易在使用时出问题的地方；每一条都在拥有该代码的任务里配了对应测试。

1. **字符串里的 `//` 或 `/*`** 不得被当作注释起点（`"a//b"`、`"x/*y"`）。
2. **未闭合字符串**必须在行尾结束（AngelScript 字符串不跨行），不能吞掉后续行。
3. **块注释跨行状态**：第 N 行 `/*` 打开、第 N+K 行 `*/` 关闭，中间各行整行着色，关闭行在 `*/` 之后恢复正常着色。
4. **类型优先于函数**：`Vector2(...)` 是 `ENGINE_TYPE`（若 `Vector2` 已注册）而不是 `FUNCTION`；`int(...)` 是 `BUILTIN_TYPE`。
5. **关键字大小写敏感**：`int` 是基本类型，`Int` 是普通标识符。
6. **空行 / 纯空白行 / 纯符号行**不产出 token 且不崩溃；`_get_line_syntax_highlighting_impl` 对空行返回空字典。

---

### Task 1: 词法器骨架 —— 标识符、关键字、基本类型

建立 `ASHighlighterLexer` 的公开接口与主循环；本任务只识别标识符（含关键字/基本类型判定），其余字符先跳过（后续任务逐个补上分支）。

**Files:**
- Create: `modules/angelscript/editor/as_highlighter_lexer.h`
- Create: `modules/angelscript/editor/as_highlighter_lexer.cpp`
- Create: `modules/angelscript/tests/test_angelscript_highlighter.h`
- Create: `modules/angelscript/tests/test_angelscript_highlighter.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `enum class ASTokenType { KEYWORD, BUILTIN_TYPE, ENGINE_TYPE, USER_TYPE, FUNCTION, MEMBER, STRING, NUMBER, COMMENT, DIRECTIVE, SYMBOL, IDENTIFIER };`
  - `struct ASToken { int start = 0; int end = 0; ASTokenType type = ASTokenType::IDENTIFIER; };`（`end` 为开区间，`[start, end)`）
  - `class ASHighlighterLexer`，静态方法：
    - `static const HashSet<String> &get_keywords();`
    - `static const HashSet<String> &get_builtin_types();`
    - `static Vector<ASToken> tokenize(const String &p_line, bool p_in_block_comment, const HashSet<StringName> &p_engine_types, const HashSet<StringName> &p_user_types, bool &r_out_block_comment);`

- [ ] **Step 1: 写失败的测试**

创建 `modules/angelscript/tests/test_angelscript_highlighter.h`：

```cpp
/**************************************************************************/
/*  test_angelscript_highlighter.h                                        */
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

#include "tests/test_macros.h"

// 本头被两处 include：tests/test_main.cpp（注册用例）与同目录的
// test_angelscript_highlighter.cpp（拿到函数声明）。实现方必须先定义
// ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL，否则 TEST_CASE 会注册两次。
void as_highlighter_lexer_recognizes_keywords_and_builtin_types();

#ifndef ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL

TEST_CASE("[AngelScript] highlighter lexer recognizes keywords and builtin types") {
	as_highlighter_lexer_recognizes_keywords_and_builtin_types();
}

#endif // ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL
```

创建 `modules/angelscript/tests/test_angelscript_highlighter.cpp`：

```cpp
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
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmd /c "build-tests.cmd > %TEMP%\as-plan.log 2>&1"`（或 `.superpowers\sdd\2026-09-30-angelscript-hot-update-phase1\build-tests.cmd`）
Expected: 编译失败（`editor/as_highlighter_lexer.h` 不存在 / `ASHighlighterLexer` 未定义）。

- [ ] **Step 3: 写最小实现**

创建 `modules/angelscript/editor/as_highlighter_lexer.h`：

```cpp
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
```

创建 `modules/angelscript/editor/as_highlighter_lexer.cpp`：

```cpp
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
		i++;
	}

	// 块注释状态在 Task 3 处理；此处先保持中立。
	r_out_block_comment = false;
	return tokens;
}
```

- [ ] **Step 4: 运行测试确认通过**

Run: `cmd /c "build-tests.cmd > %TEMP%\as-plan.log 2>&1"`，再 `cmd /c "bin\godot.windows.editor.x86_64.console.exe --headless --test --test-case="*highlighter*" > %TEMP%\as-hl.log 2>&1"`。
Expected: `1 | 1 passed | 0 failed`。

再跑一次全套确保没破坏既有用例：`--test-case="*AngelScript*"`，Expected: 全绿（数量 = 原 99 + 1）。

- [ ] **Step 5: 提交**

使用 `git-commit` 技能，消息建议：`feat(angelscript): add highlighter lexer skeleton with keywords and builtin types`。

---

### Task 2: 词法器 —— 字符串、数字、符号、行注释、块注释、指令

**Files:**
- Modify: `modules/angelscript/editor/as_highlighter_lexer.cpp`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.h`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.cpp`

**Interfaces:**
- Consumes: Task 1 的 `ASHighlighterLexer::tokenize` 及其签名。
- Produces: 同一签名，语义扩展为完整识别字符串/数字/符号/注释/指令，并正确写出 `r_out_block_comment`。

- [ ] **Step 1: 写失败的测试**

在 `test_angelscript_highlighter.h` 的 `void as_highlighter_lexer_recognizes_keywords_and_builtin_types();` 之后追加声明：

```cpp
void as_highlighter_lexer_recognizes_literals_comments_and_directives();
```

在 `#endif` 之前追加注册：

```cpp
TEST_CASE("[AngelScript] highlighter lexer recognizes literals, comments and directives") {
	as_highlighter_lexer_recognizes_literals_comments_and_directives();
}
```

在 `test_angelscript_highlighter.cpp` 末尾追加实现：

```cpp
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
	CHECK(first_token_text(" end */ x", ASTokenType::COMMENT) == " end */");
	CHECK(count_tokens(" end */ x", ASTokenType::IDENTIFIER) == 1); // x

	// 已在块注释内的整行（无闭合）仍为注释且 out 保持 true。
	Vector<ASToken> mid_tokens = tokenize_default("still inside", true, &out);
	CHECK(out == true);
	CHECK(mid_tokens.size() == 1);
	CHECK(mid_tokens[0].type == ASTokenType::COMMENT);

	// 空行 / 纯空白不产出 token。
	CHECK(tokenize_default("").is_empty());
	CHECK(tokenize_default("   ").is_empty());
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: 构建 + `--test-case="*literals*"`。
Expected: FAIL（字符串/数字/注释均未识别）。

- [ ] **Step 3: 写实现**

把 `as_highlighter_lexer.cpp` 主循环里的「后续任务补齐」占位替换为以下完整分支（放在 `_is_ident_start` 分支之前、`while` 循环体内，并删除末尾的 `r_out_block_comment = false;` 改为使用局部 `in_block`）：

在 `int i = 0;` 之后加入：

```cpp
	bool in_block = p_in_block_comment;
```

然后主循环体依次为：

```cpp
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
```

在数字分支之后、原来的标识符分支保留不动；在标识符分支之后（`i++` 之前）加入符号分支与收尾：

```cpp
		// 6) 符号（单字符）。
		if (is_symbol(c)) {
			tokens.push_back({ i, i + 1, ASTokenType::SYMBOL });
			i++;
			continue;
		}
```

把函数末尾的 `r_out_block_comment = false;` 改为：

```cpp
	r_out_block_comment = in_block;
```

> 注意：`is_symbol` 来自 `core/string/char_utils.h`，它把空格/制表也算作 symbol。因此符号分支必须放在「空白跳过」之前会误判空格；实际顺序应为——先 `if (is_symbol(c))` 只对非空白成立？为避免空格被标成符号，符号判定改用显式集合：在文件顶部加
> ```cpp
> static bool _is_as_symbol(char32_t p_char) {
> 	return p_char != '_' && !is_ascii_identifier_char(p_char) && !is_whitespace(p_char) && p_char != '"';
> }
> ```
> 并把分支条件写成 `if (_is_as_symbol(c))`。这样空白与已处理的字符串引号都不会进入符号分支。

- [ ] **Step 4: 运行测试确认通过**

Run: 构建 + `--test-case="*literals*"`，Expected: PASS。
再跑 `--test-case="*AngelScript*"`，Expected: 全绿。

- [ ] **Step 5: 提交**

使用 `git-commit` 技能，消息建议：`feat(angelscript): lex strings, numbers, comments and directives in the highlighter`。

---

### Task 3: 词法器 —— 引擎类型 / 用户类型 / 函数 / 成员判定

**Files:**
- Modify: `modules/angelscript/editor/as_highlighter_lexer.cpp`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.h`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.cpp`

**Interfaces:**
- Consumes: Task 1/2 的 `tokenize`。
- Produces: 标识符分支新增 `p_engine_types`/`p_user_types` 命中判定、后随 `(` 判 `FUNCTION`、前接 `.` 判 `MEMBER`，且**类型判定优先于函数/成员**。

- [ ] **Step 1: 写失败的测试**

在 `test_angelscript_highlighter.h` 追加声明：

```cpp
void as_highlighter_lexer_classifies_identifiers_with_type_precedence();
```

在 `#endif` 之前追加注册：

```cpp
TEST_CASE("[AngelScript] highlighter lexer classifies identifiers with type precedence") {
	as_highlighter_lexer_classifies_identifiers_with_type_precedence();
}
```

在 `test_angelscript_highlighter.cpp` 末尾追加实现（注意 `tokenize_default` 支持传引擎/用户类型集合）：

```cpp
void as_highlighter_lexer_classifies_identifiers_with_type_precedence() {
	HashSet<StringName> engine_types;
	engine_types.insert("Node");
	engine_types.insert("Vector2");

	HashSet<StringName> user_types;
	user_types.insert("MyThing");

	// 引擎类型 / 用户类型。
	bool out = false;
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
```

- [ ] **Step 2: 运行测试确认失败**

Run: 构建 + `--test-case="*type precedence*"`。Expected: FAIL（引擎/用户类型、函数、成员均未识别）。

- [ ] **Step 3: 写实现**

把 `as_highlighter_lexer.cpp` 标识符分支中的判定链从：

```cpp
			if (as_keywords.has(word)) {
				type = ASTokenType::KEYWORD;
			} else if (as_builtin_types.has(word)) {
				type = ASTokenType::BUILTIN_TYPE;
			}
```

改为：

```cpp
			if (as_keywords.has(word)) {
				type = ASTokenType::KEYWORD;
			} else if (as_builtin_types.has(word)) {
				type = ASTokenType::BUILTIN_TYPE;
			} else if (p_engine_types.has(word)) {
				type = ASTokenType::ENGINE_TYPE;
			} else if (p_user_types.has(word)) {
				type = ASTokenType::USER_TYPE;
			} else if (i > 0 && p_line[i - 1] == '.') {
				// 前接 '.' → 成员访问。放在类型判定之后，保证 `this.Node` 之类仍优先判类型。
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
```

- [ ] **Step 4: 运行测试确认通过**

Run: 构建 + `--test-case="*type precedence*"`，Expected: PASS；再 `--test-case="*AngelScript*"`，Expected: 全绿。

- [ ] **Step 5: 提交**

使用 `git-commit` 技能，消息建议：`feat(angelscript): classify engine/user types, functions and members in the highlighter`。

---

### Task 4: 高亮器本体、编辑器接入与注册

把 token 映射成主题色，维护跨行块注释缓存，并把高亮器注册进 `ScriptEditor`。

**Files:**
- Create: `modules/angelscript/editor/as_syntax_highlighter.h`
- Create: `modules/angelscript/editor/as_syntax_highlighter.cpp`
- Modify: `modules/angelscript/SCsub`
- Modify: `modules/angelscript/register_types.cpp`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.h`
- Modify: `modules/angelscript/tests/test_angelscript_highlighter.cpp`

**Interfaces:**
- Consumes: `ASHighlighterLexer::tokenize`、`ASTokenType`、`ASToken`；引擎侧 `EditorSyntaxHighlighter`、`EDITOR_GET`、`ScriptEditor::get_singleton()->register_syntax_highlighter()`、`EditorNode::add_init_callback()`。
- Produces: `class ASSyntaxHighlighter : public EditorSyntaxHighlighter`，`_get_name()` 返回 `"AngelScript"`，`_get_supported_languages()` 返回 `{"AngelScript"}`。

- [ ] **Step 1: 写失败的测试**

在 `test_angelscript_highlighter.h` 顶部 `#include "tests/test_macros.h"` 之后追加（仅编辑器构建可见）：

```cpp
#ifdef TOOLS_ENABLED
void as_syntax_highlighter_reports_language_and_creates_instances();
#endif
```

在 `#endif // ANGELSCRIPT_HIGHLIGHTER_TESTS_IMPL` 之前追加：

```cpp
#ifdef TOOLS_ENABLED
TEST_CASE("[AngelScript] syntax highlighter reports its language and creates instances") {
	as_syntax_highlighter_reports_language_and_creates_instances();
}
#endif
```

在 `test_angelscript_highlighter.cpp` 顶部追加（仅编辑器构建）：

```cpp
#ifdef TOOLS_ENABLED
#include "editor/as_syntax_highlighter.h"
#endif
```

并在文件末尾追加：

```cpp
#ifdef TOOLS_ENABLED
void as_syntax_highlighter_reports_language_and_creates_instances() {
	Ref<ASSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	REQUIRE(highlighter.is_valid());

	CHECK(highlighter->_get_name() == "AngelScript");
	CHECK(highlighter->_get_supported_languages().has("AngelScript"));

	Ref<EditorSyntaxHighlighter> created = highlighter->_create();
	CHECK(created.is_valid());
}
#endif
```

- [ ] **Step 2: 运行测试确认失败**

Run: 构建 + `--test-case="*syntax highlighter*"`。Expected: 编译失败（`editor/as_syntax_highlighter.h` 不存在，且 SCsub 未编 `editor/`）。

- [ ] **Step 3: 写实现**

创建 `modules/angelscript/editor/as_syntax_highlighter.h`（顶部复制 Godot MIT 许可证注释块）：

```cpp
#pragma once

#include "as_highlighter_lexer.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/script/syntax_highlighters.h"

// 把 ASHighlighterLexer 的 token 映射成编辑器主题色。
// 跨行块注释状态用与 GDScriptSyntaxHighlighter/CodeHighlighter 相同的「按行缓存 + 向前走查」策略，
// 而不是在 spec 里设想的 block_comment_cached_to：走查法已被上游验证，且不需要额外字段。
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
```

创建 `modules/angelscript/editor/as_syntax_highlighter.cpp`（顶部复制许可证注释块）：

```cpp
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
		const bool *state = block_comment_state.get(p_line - 1);
		in_block_comment = state != nullptr ? *state : false;
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
```

修改 `modules/angelscript/SCsub`：在 `env_as.add_source_files(env.modules_sources, "binding/*.cpp")` 之后新增：

```python
if env.editor_build:
    env_as.add_source_files(env.modules_sources, "./editor/*.cpp")
```

修改 `modules/angelscript/register_types.cpp`：

1. 在文件顶部 include 区末尾追加：

```cpp
#ifdef TOOLS_ENABLED
#include "editor/as_syntax_highlighter.h"
#include "editor/editor_node.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/script/syntax_highlighters.h"
#endif
```

2. 在 `initialize_angelscript_module` 函数体最前面加入编辑器级注册分支，并让 SERVERS 分支末尾挂上初始化回调：

```cpp
void initialize_angelscript_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(ASSyntaxHighlighter);
		return;
	}
#endif

	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}

	// ……（原有 SERVERS 逻辑保持不变）……

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(_as_editor_init);
#endif
}
```

3. 在文件中、`initialize_angelscript_module` 之前加入：

```cpp
#ifdef TOOLS_ENABLED
// 编辑器初始化回调：在 ScriptEditor 就绪后注册 AS 语法高亮器。
static void _as_editor_init() {
	Ref<ASSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
}
#endif
```

> 若原函数体的 SERVERS 逻辑块尾部已有 `return` 或位于 `if` 块内，请把 `EditorNode::add_init_callback(_as_editor_init);` 放在该 SERVERS 逻辑**成功执行之后**的同一路径上（与 GDScript 的 `register_types.cpp` 位置一致）。

- [ ] **Step 4: 运行测试确认通过**

Run: 构建（`build-tests.cmd`）+ `--test-case="*syntax highlighter*"`，Expected: `1 | 1 passed | 0 failed`。
再 `--test-case="*AngelScript*"`，Expected: 全绿（数量 = 原 99 + 2）。
再手工验收：启动 `bin\godot.windows.editor.x86_64.mono.exe`（若尚未重建则先用当前编辑器二进制），打开任一 `.as`（如 e2e 工程的 `module/angelscript/tests/e2e_project/HelloAS.as`），确认关键字/类型/字符串/数字/注释/指令有颜色；切换编辑器主题后颜色跟随。

- [ ] **Step 5: 提交**

使用 `git-commit` 技能，消息建议：`feat(angelscript): register editor syntax highlighter`。

---

### Task 5: 全量回归、mono 重建与验收

**Files:**
- 无源码改动（若回归发现问题则回到对应任务修）。

- [ ] **Step 1: 非编辑器（测试）全量回归**

Run: `cmd /c "bin\godot.windows.editor.x86_64.console.exe --headless --test --test-case="*AngelScript*" > %TEMP%\as-final.log 2>&1"`
Expected: `0 failed`，`Status: SUCCESS!`。

- [ ] **Step 2: 重建 mono 编辑器与导出模板**

Run（后台）: `cmd /c "build-both.cmd > %TEMP%\build-both-highlighter.log 2>&1"`
Expected: `EXIT=0`，日志含 `Compiling modules\angelscript\editor\as_syntax_highlighter.cpp`、`Compiling modules\angelscript\editor\as_highlighter_lexer.cpp` 与 `Linking Program bin\godot.windows.editor.x86_64.mono.exe`。
> 注意：若 mono 编辑器正在运行会锁住 exe 导致链接「拒绝访问」，需先关闭编辑器。

- [ ] **Step 3: mono 二进制验证**

Run: `cmd /c "bin\godot.windows.editor.x86_64.mono.console.exe --headless --test --test-case="*AngelScript*" > %TEMP%\mono-as-highlighter.log 2>&1"`
Expected: `0 failed`，`SUCCESS`（证明高亮器代码已进入用户运行的 mono 二进制）。

- [ ] **Step 4: 人眼验收（交付用户）**

启动 `bin\godot.windows.editor.x86_64.mono.exe` 打开工程，逐个确认：
- `.as` 打开即着色；
- 字符串里的 `//`、`/*` 不变色；
- 多行块注释中间各行整行注释色，`*/` 后恢复正常；
- 编辑器主题切换后颜色跟随；
- `// godot_base: Node` 有注释色。
请用户确认。

- [ ] **Step 5: 提交（如 Steps 1-4 有修正）**

使用 `git-commit` 技能，消息建议：`fix(angelscript): <具体修正>`；若无改动则跳过本步。

---

## Self-Review（已执行）

- **Spec 覆盖**：spec §3 组件（词法器 + 高亮器 + 测试）→ Task 1–4；§4 词法规则 → Task 1–3；§5 颜色映射 → Task 4；§6 跨行缓存 → Task 4；§7 注册/SCsub → Task 4；§8 测试与验收 → 每个任务的 Step 4 + Task 5。非目标（折叠/缩进/补全/调试）无任务，符合 spec。
- **占位符扫描**：无 TBD/TODO；所有代码步骤都给了可直接粘贴的代码。
- **类型一致性**：`ASTokenType` / `ASToken` / `tokenize` 签名在 Task 1 定义，Task 2–4 原样使用；`_token_color`、`block_comment_state`、`_update_cache` 等在 Task 4 内自洽。
- **偏离 spec 之处（已记录）**：spec §6/§3 提到字段 `block_comment_cached_to`，实现改用 `CodeHighlighter` 式走查缓存（`block_comment_state`），无需该字段——以 `as_syntax_highlighter.h` 注释说明。
- **Review Focus 覆盖**：字符串内注释 → Task 2；未闭合字符串 → Task 2；块注释跨行 → Task 2；类型优先于函数 → Task 3；关键字大小写 → Task 1；空行 → Task 2 + Task 4 的空行早退。
