# AngelScript 编辑器语法高亮设计（Token 着色）

- 日期：2026-10-09
- 状态：设计已评审，待写实现计划
- 关联：`docs/superpowers/specs/2026-09-30-godot-angelscript-hot-update-design.md`（总设计；其 §13 将该工作列为 M8 编辑器体验的第一项）
- 范围：**仅 Token 着色**。不含代码折叠、自动缩进、括号/引号成对、补全、跳转、大纲、错误行标记、断点调试（B 调试另行设计）。

## 1. 背景与目标

### 1.1 现状

- `modules/angelscript/` 为 Godot 4.7 的 AngelScript 语言模块，M0–M5（语言/绑定层/对象模型/`.asb` 字节码/宿主桥）已落地。
- `modules/angelscript/README.md:23` 明确“编辑器语言服务与断点调试尚未提供”。
- 模块内**没有任何** `SyntaxHighlighter` 集成；打开 `.as` 时编辑器只能回退到通用纯文本着色，关键字、类型、字符串、注释均无区分。

### 1.2 目标

让 `.as` 文件在 Godot 内置脚本编辑器中获得与 GDScript 观感一致的 Token 着色，覆盖：

关键字、AngelScript 基本类型、引擎类型（ClassDB 暴露类型）、用户全局类、函数名、成员名、字符串、数字、注释、符号，以及模块约定的 `// godot_base:` 指令。

### 1.3 非目标（YAGNI，明确不做）

代码折叠、自动缩进、括号/引号自动成对、注释快捷键、代码补全、定义跳转、函数/类大纲、错误行标记、断点/单步/调用栈（B 调试子系统）。

这些能力最终会启用 Godot `ScriptLanguage`/`CodeEdit` 的其它扩展点，但**本 spec 只交付 Token 着色**，避免范围蔓延。

## 2. 约束

- 仅在 `TOOLS_ENABLED`（编辑器构建）下编译与生效，不改变运行时行为。
- 复用编辑器主题色，不硬编码颜色；跟随主题切换。
- 与既有 AS 模块的注册/构建方式保持一致，不引入新的第三方依赖。
- 词法逻辑与引擎解耦，可单元测试。

## 3. 架构与组件

### 3.1 文件布局

| 文件 | 角色 |
| --- | --- |
| `modules/angelscript/editor/as_highlighter_lexer.{h,cpp}` | 纯词法器：逐行把源码切成带类别的 token 区间；无引擎/编辑器依赖 |
| `modules/angelscript/editor/as_syntax_highlighter.{h,cpp}` | `ASSyntaxHighlighter : EditorSyntaxHighlighter`（GDCLASS）：颜色映射、多行注释状态缓存、把 token 转成 TextEdit 所需的 `{列: {color}}` 映射 |
| `modules/angelscript/tests/test_angelscript_highlighter.{h,cpp}` | 词法器单元测试（纯函数，可在 headless 测试中运行） |
| `modules/angelscript/register_types.cpp` | 新增编辑器注册（高亮器实例注册 + 类注册） |
| `modules/angelscript/SCsub` | 编辑器构建时把 `./editor/*.cpp` 编入模块 |

> 说明：`as_tokenizer.{h,cpp}`（AngelScript 自带）位于 `thirdparty/angelscript/source/`，不在模块 include 路径上，且其 API/头耦合第三方内部结构。为保持对第三方零侵入与可测性，本 spec 选择**自研轻量词法器**（方案 2），其行为不依赖 AngelScript 内部实现。

### 3.2 词法器接口

```cpp
enum class ASTokenType {
	KEYWORD,      // AngelScript 保留字（if/for/class/return/...）
	BUILTIN_TYPE, // AngelScript 基本类型（int/float/bool/string/void/...）
	ENGINE_TYPE,  // ClassDB 中暴露的引擎类型（Node/Vector2/...）
	USER_TYPE,    // 用户全局类（ScriptServer 全局类列表）
	FUNCTION,     // 标识符，且其后（跳过空白）为 '('
	MEMBER,       // '.' 之后的标识符
	STRING,
	NUMBER,
	COMMENT,
	DIRECTIVE,    // 形如 "// godot_base:" 的模块指令
	SYMBOL,
	IDENTIFIER,   // 无法进一步归类的普通标识符
};

struct ASToken {
	int start; // 起始列（含）
	int end;   // 结束列（不含）
	ASTokenType type;
};

class ASHighlighterLexer {
public:
	static const HashSet<String> &get_keywords();
	static const HashSet<String> &get_builtin_types();

	// p_in_block_comment：本行行首是否处于块注释内
	// p_engine_types / p_user_types：由高亮器在 _update_cache() 构建后传入
	// r_out_block_comment：本行结束时是否仍处于块注释内
	static Vector<ASToken> tokenize(
			const String &p_line,
			bool p_in_block_comment,
			const HashSet<StringName> &p_engine_types,
			const HashSet<StringName> &p_user_types,
			bool &r_out_block_comment);
};
```

词法器是**纯函数**：给定输入一定得到相同输出，不读全局状态，便于单元测试。引擎类型/用户类型集合以参数注入，测试时可传空集或以自定义集合验证优先级。

### 3.3 高亮器接口

```cpp
class ASSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(ASSyntaxHighlighter, EditorSyntaxHighlighter)

	Color symbol_color, keyword_color, base_type_color, engine_type_color;
	Color user_type_color, comment_color, string_color, number_color;
	Color function_color, member_variable_color, font_color;

	HashSet<StringName> engine_types; // ClassDB::get_class_list + is_class_exposed
	HashSet<StringName> user_types;   // ScriptServer 全局类列表

	HashMap<int, bool> block_comment_state; // 行号 -> 行首是否在块注释
	int block_comment_cached_to = -1;

protected:
	void _update_cache() override;
	Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
	void _clear_highlighting_cache() override;
	Ref<EditorSyntaxHighlighter> _create() const override;
	String _get_name() const override;
	PackedStringArray _get_supported_languages() const override;

public:
	ASSyntaxHighlighter();
};
```

- `_get_name()` 返回 `"AngelScript"`；`_get_supported_languages()` 返回 `{"AngelScript"}`（与 `ASScriptLanguage::get_name()` 一致，编辑器据此匹配 `.as` 脚本）。
- `_create()` 返回 `Ref<ASSyntaxHighlighter>`（编辑器为每个脚本编辑器实例克隆高亮器）。

## 4. 词法器规则

逐列线性扫描一行，规则按以下优先级匹配（先匹配者胜出）：

1. **注释**
   - `//` 起至行尾为行注释。若其内容（跳过 `//` 与空白后）以 `godot_base:` 开头，则整段归类为 `DIRECTIVE`，否则 `COMMENT`。
   - `/*` 起为块注释，直到遇到 `*/`；若本行未闭合，则 `r_out_block_comment = true`，并使下一行行首处于块注释。AS 不支持嵌套块注释。
2. **字符串**：`"` 开始，支持 `\` 转义，直到下一个未转义的 `"`；AngelScript 字符串不跨行，若行内未闭合则到行尾结束。
3. **数字**：以数字开头，支持 `0x`/`0b` 前缀、十进制整数、浮点与指数、类型后缀（如 `f`/`d`/`u`）。
4. **标识符**：`[A-Za-z_][A-Za-z0-9_]*`，然后按顺序判定：
   - 命中 `get_keywords()` → `KEYWORD`
   - 命中 `get_builtin_types()` → `BUILTIN_TYPE`
   - 命中 `p_engine_types` → `ENGINE_TYPE`
   - 命中 `p_user_types` → `USER_TYPE`
   - 其后（跳过同行空白）为 `(` → `FUNCTION`
   - 紧邻前一个非空 token 为 `.` 或 `?.`（句柄成员访问）→ `MEMBER`
   - 否则 → `IDENTIFIER`
   - 优先级说明：关键字、基本类型优先于引擎/用户类型（例如不会把 `bool` 当成引擎类）；类型判定优先于函数/成员判定（因此 `Vector2(...)` 按类型着色）。其余情况再按“后随 `(` 记函数、前接 `.` 记成员”归类。
5. **符号**：其余可打印符号（`+-*/%=<>!&|^~?:;,.()[]{}@` 等）逐个归类为 `SYMBOL`。
6. **空白**：不产出 token（但参与“跳过空白”的判定）。

关键字表（AngelScript 2.38 保留字，去重后）：`abstract, and, break, case, cast, class, const, continue, default, do, else, enum, false, final, for, from, funcdef, get, if, import, in, inout, interface, is, mixin, namespace, not, null, or, out, override, private, protected, return, set, shared, switch, this, true, typedef, while, xor`。

基本类型表：`auto, bool, int, int8, int16, int32, int64, uint, uint8, uint16, uint32, uint64, float, double, string, void`。

> 最终表由代码中的常量集合定义，本 spec 的表用于对齐预期；新增语言版本保留字时同步维护。

## 5. 颜色映射

高亮器把每个 `ASToken` 映射到编辑器主题键，再产出 `Dictionary{列: {"color": Color}}`：

| Token | 主题键 |
| --- | --- |
| `SYMBOL` | `text_editor/theme/highlighting/symbol_color` |
| `KEYWORD` | `.../keyword_color` |
| `BUILTIN_TYPE` | `.../base_type_color` |
| `ENGINE_TYPE` | `.../engine_type_color` |
| `USER_TYPE` | `.../user_type_color` |
| `COMMENT` | `.../comment_color` |
| `DIRECTIVE` | `.../comment_color`（保留独立类别；首版观感同注释，未来若引入专用键可单独着色） |
| `STRING` | `.../string_color` |
| `NUMBER` | `.../number_color` |
| `FUNCTION` | `.../function_color` |
| `MEMBER` | `.../member_variable_color` |
| `IDENTIFIER` | `font_color`（不显式写入，回退默认前景色） |

- 颜色来源：`EDITOR_GET("text_editor/theme/highlighting/<key>")`（定义见 `editor/settings/editor_settings.h`）。
- `font_color`：`text_edit->get_theme_color(SceneStringName(font_color))`。
- 产出方式与 GDScript 一致：只在颜色**变化**的列写入一个点，减少字典体积。

## 6. 多行块注释状态

- 高亮器维护 `block_comment_state`（行号 → 行首是否处于块注释）与 `block_comment_cached_to`。
- 请求第 `N` 行时：若 `N <= block_comment_cached_to` 直接取缓存；否则从 `block_comment_cached_to + 1` 起用词法器逐行推进（利用每行的 `r_out_block_comment`），直到 `N`，并把中间结果写回缓存。这样连续渲染为均摊线性，避免每次 O(N) 重扫。
- `_clear_highlighting_cache()`（编辑器文本变化时由基类/TextEdit 触发）清空 `block_comment_state` 与 `block_comment_cached_to`，保证编辑后重算。
- 单测覆盖词法器的 `r_out_block_comment` 语义（进入/退出、行中闭合、未闭合到行尾）。

## 7. 注册与生命周期

### 7.1 `register_types.cpp`

保持现状的 `MODULE_INITIALIZATION_LEVEL_SERVERS` 分支不变（语言/加载器/保存器注册、`script_language_as != nullptr` 幂等守卫），并新增：

- 在 `#ifdef TOOLS_ENABLED` 下，于 SERVERS 初始化里 `EditorNode::add_init_callback(_as_editor_init)`（与 GDScript 相同模式），回调内：
  ```cpp
  Ref<ASSyntaxHighlighter> highlighter;
  highlighter.instantiate();
  ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
  ```
- 新增 `MODULE_INITIALIZATION_LEVEL_EDITOR` 分支：`GDREGISTER_CLASS(ASSyntaxHighlighter);`。

新增 include（仅 `TOOLS_ENABLED` 下需要）：`editor/editor_node.h`、`editor/script/script_editor_plugin.h`、`editor/script/syntax_highlighters.h`、`editor/settings/editor_settings.h`，以及 `editor/as_syntax_highlighter.h`。

### 7.2 `SCsub`

在既有 `env_as` 配置后追加（对齐 `modules/gdscript/SCsub`）：

```python
if env.editor_build:
    env_as.add_source_files(env.modules_sources, "./editor/*.cpp")
```

### 7.3 卸载

沿用模块现状：编辑器高亮器随编辑器生命周期结束而释放，无需在 `uninitialize_angelscript_module` 里显式注销。

## 8. 测试策略

### 8.1 自动化（词法器，headless 可跑）

新增 `modules/angelscript/tests/test_angelscript_highlighter.{h,cpp}`，用例至少覆盖：

- 关键字与基本类型识别（`if`→KEYWORD、`int`→BUILTIN_TYPE）。
- 引擎类型/用户类型识别与优先级（传入自定义集合；`bool` 不应被判定为引擎类型）。
- 函数名识别（`foo(`）与成员识别（`.bar`）。
- 字符串：含 `\` 转义、`"` 内出现 `//` 不当作注释、行内未闭合。
- 行注释与 `// godot_base:` 指令区分。
- 块注释：行内闭合不延续；未闭合时 `r_out_block_comment == true`；下一行整行为注释类。
- 数字：十进制、`0x`、`0b`、浮点、后缀。
- 符号与普通标识符的区分。

测试经 `modules/modules_tests.gen.h` 自动收集，命令：
`bin\godot.windows.editor.x86_64.console.exe --headless --test --test-case="*AngelScript*"`。

### 8.2 人工验收

高亮器本体依赖 `TextEdit`/`EditorSettings`，不做自动化单测（与 GDScript 现状一致）。验收：

1. 打开 e2e 工程（或任意工程）中的 `.as` 文件；
2. 关键字、基本类型、引擎类型、字符串、数字、注释、符号、函数名、成员名着色正确；
3. `// godot_base:` 指令行为符合预期；
4. 切换编辑器主题（Editor Settings → Theme）后颜色随之变化；
5. 编辑文本（含删除 `*/`）时着色即时、无残留错色。

## 9. 风险与缓解

| 风险 | 缓解 |
| --- | --- |
| 词法边界错误（字符串/块注释/转义） | 纯函数 + 单元测试覆盖边界；颜色映射只在 token 边界写点，错误不扩散 |
| `_update_cache()` 构建引擎类型集合较大（ClassDB 全类型） | 一次性构建 `HashSet`，与 GDScript 同量级，可接受 |
| 多行块注释增量缓存出错导致串色 | `_clear_highlighting_cache()` 全清；缓存推进逻辑单测覆盖 |
| 主题键缺失/改名 | 使用与 GDScript 相同的键；缺失时回退默认色而非崩溃 |

## 10. 验收标准

- `.as` 文件在编辑器中出现符合 §5 映射的 Token 着色。
- §8.1 全部用例通过（绿）；AS 模块既有测试无回归。
- 编辑器构建（`target=editor`）与 mono 双支持构建（`build-both.cmd`）均通过。
- 不修改任何运行时行为；`TOOLS_ENABLED` 之外零影响。

## 11. 与后续子系统的关系

- **B 调试**：将实现 `ScriptLanguage::debug_*`、断点/单步与调用栈，需要运行时挂起模型，另行 spec。
- **C 性能分析**：将实现 `ScriptLanguage::profiling_*` 并接入编辑器 Profiler，另行 spec。
- 本 spec 的高亮器为独立编辑器组件，不阻塞也不依赖 B/C。
