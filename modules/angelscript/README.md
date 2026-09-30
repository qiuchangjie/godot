# Godot AngelScript 模块

基于 [AngelScript](https://www.angelcode.com/angelscript/) 2.38.0 的脚本语言模块，用于「主包 NativeAOT + 逻辑热更」方案中的热更通道：
游戏逻辑以 `.as` 资源下发，由本模块在运行时编译执行。设计文档见
`docs/superpowers/specs/2026-09-30-godot-angelscript-hot-update-design.md`（当前仅存在于工作区，未随仓库提交）。

## 当前状态（阶段一：M0–M1）

已可用：

- `ASScriptLanguage`：向 `ScriptServer` 注册 `AngelScript` 语言，扩展名 `as`；引擎懒初始化（`--test`/headless 下不会主动调用 `ScriptServer::init_languages()` 的路径也能工作）。
- `ASEngine`：持有唯一的 `asIScriptEngine`，提供模块编译、函数执行与最小内建 API 注册。
- `ASScript`：`.as`/`.asb` 资源（`Script` 子类），经 `ASResourceFormatLoaderASScript` / `ASResourceFormatSaverASScript` 接入资源系统，`.tscn` 可按路径引用。
- `ASScriptInstance`：脚本实例，支持内建标量属性的读写与方法按名字调用；`_ready`/`_enter_tree`/`_exit_tree`/`_process`/`_physics_process` 经 Node 的 `GDVIRTUAL` 按名字派发，`notification()` 承载 `_notification(int)`。

尚未提供（后续里程碑）：跨语言绑定层（引擎 API 编组）、字符串/对象类型、协程（await）、编辑器语言服务与断点调试（`validate()` / `find_function()` / `make_function()` 等暂为 stub）、`.asb` 预编译产物的生成工具。

## 脚本约定

- 扩展名：`.as` 为唯一可加载形态；`.asb`（预编译字节码）阶段一只保留扩展名识别，加载器会明确拒绝（二进制读写与版本校验属后续里程碑）。
- 加载线程：`.as` 必须在主线程加载（AS 引擎主线程独占，见 `as_engine.h`）；`ResourceLoader.load_threaded_request()` 会失败并打印错误，而不是在后台线程破坏引擎状态。
- 基类指令：源码前 10 行内必须有一行 `// godot_base: <ClassDB 类型名>`，它决定 `ASScript::get_instance_base_type()`（如 `Node`、`Resource`）。缺少或类型不存在都会导致加载失败。
- 类名必须等于文件名（不含扩展名），例如 `res://enemy_spawner.as` 里的类必须叫 `enemy_spawner`。本模块不做全局类名注册，脚本一律按路径引用。
- 回调签名必须与 Godot 一致：`void _ready()`、`void _process(double delta)`、`void _physics_process(double delta)`、`void _enter_tree()`、`void _exit_tree()`、`void _notification(int what)`。签名不匹配的回调不会被派发（例如 `void _process()` 视为不存在）。
- 属性：阶段一支持 `bool` / `int` / `int64` / `float` / `double`；方法参数与返回值支持同样的标量类型（`int` 形参按 32 位、`int64` 按 64 位编组）。
- 回调形参：需要对象或字符串形参的回调（`_input` / `_shortcut_input` / `_unhandled_input` / `_unhandled_key_input` / `_get_configuration_warnings` 等）在阶段一不会被派发（`_process` 这类标量形参除外）；补全它们需要 M2 的绑定层，当前只是"签名不匹配即视为不存在"，不会崩溃。

### 已知限制

- 基类指令扫描是逐行的字面量匹配（行首 `// godot_base:`），不做块注释解析：若 `/* ... */` 块注释内恰好有一行以 `// godot_base:` 开头，它会被当成指令。指令是作者侧的引导约定、并非外部输入，故此限制在阶段一接受；块注释感知的扫描留待 M2 编辑器集成。

示例 `res://main.as`：

```angelscript
// godot_base: Node

class main {
	int ready_count = 0;

	void _ready() {
		ready_count = 42;
		as_log_int(ready_count);
	}
}
```

## 内建 API（阶段一）

| 声明 | 说明 |
| --- | --- |
| `void as_log_int(int value)` | 把整数打印到 stdout。用于验证内建注册与调用，后续绑定层会替换为正式的日志/引擎 API。 |

内建函数统一使用泛型调用约定（`asCALL_GENERIC`），与后续绑定层保持同一形态；字符串类型需要 `RegisterStringFactory` 并与 Godot 的 `String` 编组，属 M2 范围。

## 构建

需要 MSVC x64（含 `ml64.exe`，由 `vcvars64.bat` 提供）与 SCons：

```
scons platform=windows target=editor tests=yes accesskit=no d3d12=no -j8
```

- `accesskit=no d3d12=no` 用于本机缺少对应依赖的场景；有其他平台需求请按需调整。
- MSVC x64 无法内联汇编调用桩，模块用 `ml64` 单独汇编 `thirdparty/angelscript/source/as_callfunc_x64_msvc_asm.asm`；GNUC 平台还需汇编 `as_callfunc_x64_gcc.S` 并关闭严格别名优化（留待平台里程碑）。

## 测试

```
bin\godot.windows.editor.x86_64.console.exe --headless --test --test-case="*AngelScript*"
```

- doctest 过滤需要用名字通配符（`*AngelScript*`），标签形式 `[AngelScript]` 匹配不到。
- 用例注册写在 `tests/*.h`（经自动生成的 `modules/modules_tests.gen.h` 由 `tests/test_main.cpp` 展开），断言与辅助实现放在 `tests/*.cpp`；后者必须先 `#define <文件对应的>_TESTS_IMPL` 再 include 测试头，否则同一批 `TEST_CASE` 会被注册两遍。
- 本仓 doctest 以无异常模式编译，`REQUIRE` 失败不会中断用例，因此测试辅助函数在每个前置条件后都要显式 `return`（否则实现退化时会以空指针崩溃收场）。

## 端到端验收（headless）

```
bin\godot.windows.editor.x86_64.console.exe --headless --path modules/angelscript/tests/e2e_project --quit-after 2
```

预期退出码为 0，stdout 出现 `42`（`main.as` 的 `_ready()` 里调用 `as_log_int`），且没有 `Failed to load AngelScript` / `AngelScript build failed` 之类的错误。这一次运行覆盖：`.as` 加载器注册、`.tscn` 按路径挂载脚本、`get_instance_base_type()` 生效、`_ready` 经 `GDVIRTUAL` → `callp()` 派发、内建 API 可调用。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `as_script_language.h/.cpp` | `ScriptLanguage` 实现（单例、语言名 `AngelScript`、扩展名 `as`）。 |
| `as_engine.h/.cpp` | AS 引擎持有者：懒初始化、模块编译、`execute`/`call_function`、内建注册。 |
| `as_script.h/.cpp` | `Script` 实现（`.as` 资源：基类指令解析、类名校验、编译）。 |
| `as_script_instance.h/.cpp` | `ScriptInstance` 实现（实例创建、属性读写、方法/通知派发）。 |
| `as_resource_format.h/.cpp` | `.as`/`.asb` 的资源加载器与保存器。 |
| `register_types.cpp` | 模块注册（`ScriptServer`、资源加载器/保存器）。 |
| `thirdparty/angelscript/` | AngelScript 2.38.0 官方源码（zlib 许可，见 `LICENSE.txt`）。 |
| `tests/` | doctest 用例；`tests/e2e_project/` 为 headless 验收项目。 |

## 许可

- 第三方 `thirdparty/angelscript/` 使用 zlib 许可，许可全文见 `thirdparty/angelscript/LICENSE.txt`。
- 模块自身源码沿用 Godot 的 MIT 许可头（由 `misc/scripts/copyright_headers.py` 统一维护）。
