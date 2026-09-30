# Godot AngelScript 模块

基于 [AngelScript](https://www.angelcode.com/angelscript/) 2.38.0 的脚本语言模块，用于「主包 NativeAOT + 逻辑热更」方案中的热更通道：
游戏逻辑以 `.as` 资源下发，由本模块在运行时编译执行。设计文档见
`docs/superpowers/specs/2026-09-30-godot-angelscript-hot-update-design.md`（当前仅存在于工作区，未随仓库提交）。

## 当前状态（阶段一 M0–M1 + M2 绑定层）

已可用：

- `ASScriptLanguage`：向 `ScriptServer` 注册 `AngelScript` 语言，扩展名 `as`；引擎懒初始化（`--test`/headless 下不会主动调用 `ScriptServer::init_languages()` 的路径也能工作）。
- `ASEngine`：持有唯一的 `asIScriptEngine`，提供模块编译、函数执行与内建 API 注册；`ensure_initialized()` 时按项目设置构建绑定计划并一次性注册。
- `ASScript`：`.as`/`.asb` 资源（`Script` 子类），经 `ASResourceFormatLoaderASScript` / `ASResourceFormatSaverASScript` 接入资源系统，`.tscn` 可按路径引用。
- `ASScriptInstance`：脚本实例，支持内建标量属性的读写与方法按名字调用；`_ready`/`_enter_tree`/`_exit_tree`/`_process`/`_physics_process` 经 Node 的 `GDVIRTUAL` 按名字派发，`notification()` 承载 `_notification(int)`。
- 绑定层（M2）：把 ClassDB 内省结果注册给 AS，让热更脚本能直接调用引擎 API。
  - 内建值类型：34 个内建 `Variant` 值类型（`Vector2` / `String` / `Array` / `Dictionary` / `Packed*Array` …）加 `Variant` 自身，统一以 `Variant` 为内存布局，方法/属性/运算符按内省自动注册；AS 原生 `string` 与 Godot `String` 双向转换。
  - 对象类型：`Object` 派生类以引用类型注册，含工厂（可实例化类）、方法、属性（virtual property）、常量、枚举与向上转换（`opImplCast`）；`RefCounted` 走引用计数语义，脚本持有会延长生命周期。
  - `@GlobalScope` 工具函数：签名可表达的工具函数（如 `clampf`）注册为全局函数。
  - 可见性：`angel_script/class_whitelist` / `angel_script/class_blacklist` 项目设置控制绑定范围（空白名单 = 全部放行，`Object` 无条件保留，白名单会回填祖先链）。
  - `--dump-angelscript-api <dir>`：把当前绑定面导出为 `angelscript_api.d.as`（声明，首行为内容哈希）与 `angelscript_unbound.txt`（不可绑定清单）。

尚未提供（后续里程碑）：跨语言通道的对象/字符串回调编组（`_input` 等带参回调仍不派发）、协程（await）、编辑器语言服务与断点调试（`validate()` / `find_function()` / `make_function()` 等暂为 stub）、`.asb` 预编译产物的生成工具、脚本内自定义类注册进 ClassDB。

## 脚本约定

- 扩展名：`.as` 为唯一可加载形态；`.asb`（预编译字节码）阶段一只保留扩展名识别，加载器会明确拒绝（二进制读写与版本校验属后续里程碑）。
- 加载线程：`.as` 必须在主线程加载（AS 引擎主线程独占，见 `as_engine.h`）；`ResourceLoader.load_threaded_request()` 会失败并打印错误，而不是在后台线程破坏引擎状态。
- 基类指令：源码前 10 行内必须有一行 `// godot_base: <ClassDB 类型名>`，它决定 `ASScript::get_instance_base_type()`（如 `Node`、`Resource`）。缺少或类型不存在都会导致加载失败。
- 类名必须等于文件名（不含扩展名），例如 `res://enemy_spawner.as` 里的类必须叫 `enemy_spawner`。本模块不做全局类名注册，脚本一律按路径引用。
- 回调签名必须与 Godot 一致：`void _ready()`、`void _process(double delta)`、`void _physics_process(double delta)`、`void _enter_tree()`、`void _exit_tree()`、`void _notification(int what)`。签名不匹配的回调不会被派发（例如 `void _process()` 视为不存在）。
- 属性：阶段一支持 `bool` / `int` / `int64` / `float` / `double`；方法参数与返回值支持同样的标量类型（`int` 形参按 32 位、`int64` 按 64 位编组）。
- 绑定层类型（M2）：`int`→`int64`、`float`→`double`、`StringName`/`NodePath` 等内建值类型按 `Variant` 存储的类型直接可用；对象类型必须用显式句柄语法（`Node @n = Node();`，本模块关闭了 AS 的隐式句柄），枚举写作 `<声明类>_<枚举名>::<值>`（如 `Node_ProcessMode::PROCESS_MODE_ALWAYS`），常量写作 `<声明类>_<常量名>`（如 `Object_NOTIFICATION_PREDELETE`）。
- 回调形参：需要对象或字符串形参的回调（`_input` / `_shortcut_input` / `_unhandled_input` / `_unhandled_key_input` / `_get_configuration_warnings` 等）暂不会被派发（`_process` 这类标量形参除外），因为 `ASScriptInstance::callp` 的编组目前只支持标量；绑定层已能表达对象与字符串，补全回调通道属后续里程碑。

### 已知限制

- 基类指令扫描是逐行的字面量匹配（行首 `// godot_base:`），不做块注释解析：若 `/* ... */` 块注释内恰好有一行以 `// godot_base:` 开头，它会被当成指令。指令是作者侧的引导约定、并非外部输入，故此限制在阶段一接受；块注释感知的扫描留待 M2 编辑器集成。
- 热更脚本类不是基类的子类：基类指令只决定节点实例类型（D2），脚本里的 `this` 是 AS 对象而非 Node，因此不能直接写 `add_child(...)`；绑定层 API 需要显式对象句柄（`Node @n = Node(); n.add_child(...)`）。
- 不可绑定的签名一律整体归入 unbound（不半注册），可通过 `--dump-angelscript-api` 的 `angelscript_unbound.txt` 查询：vararg 方法（含内建 `print`）、静态方法、返回或形参为 `Variant` 的方法、形参/返回引用不可见类型的方法、非标识符属性名（如 `frame_0/texture`）。
- vararg 的 `print` 家族因此不可用；热更脚本的字符串输出暂用内建 `as_log_string`（见内建 API 表）。
- 不支持向下转换（无 `opCast`）：只能把派生类句柄赋给祖先类句柄，不能反向。
- 非 `RefCounted` 的 `Object`（如 `Node`）不绑定 `free()`（它是 GDVIRTUAL、没有 MethodBind），脚本无法显式释放，退出时会出现 `ObjectDB instances were leaked` 警告；`RefCounted` 由引用计数正常回收。

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

## 内建 API

| 声明 | 说明 |
| --- | --- |
| `void as_log_int(int value)` | 把整数打印到 stdout（阶段一引入的宿主调试桩）。 |
| `void as_log_string(const String &in value)` | 把 Godot `String` 打印到 stdout。内建 `print` 是 vararg、M2 不可绑定，热更脚本的字符串输出暂用这个出口。 |

内建函数统一使用泛型调用约定（`asCALL_GENERIC`），与绑定层保持同一形态。`as_log_string` 在绑定层值类型注册之后才注册，因为它用的是绑定层的 Godot `String` 值类型。

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

## API 导出（绑定面）

```
bin\godot.windows.editor.x86_64.console.exe --headless --dump-angelscript-api <目录>
```

写出两个文件后退出：

- `angelscript_api.d.as`：当前绑定面（受 `angel_script/class_whitelist` / `class_blacklist` 影响）的声明，首行为 `// angelscript-api-version: <sha256>`，可用于判断热更脚本与当前引擎 API 是否匹配。
- `angelscript_unbound.txt`：不可绑定清单，每行 `owner<TAB>member<TAB>reason`，按字典序稳定排序（两次导出内容逐字节一致）。

## 端到端验收（headless）

```
bin\godot.windows.editor.x86_64.console.exe --headless --path modules/angelscript/tests/e2e_project --quit-after 2
```

预期退出码为 0，stdout 依次出现 `Node`、`1`、`3`、`42`：

- `42`：`main.as` 的 `_ready()` 调用 `as_log_int`（阶段一通道）。
- `Node` / `1` / `3`：`binding_demo.as` 用绑定层 API 演示 `Node.get_class()` 的 String 编组、`add_child` 的对象参数、`get_child_count` 的 int64 返回值与 `process_mode` 属性读写。

没有 `Failed to load AngelScript` / `Failed loading resource` 之类的错误。这一次运行覆盖：`.as` 加载器注册、`.tscn` 按路径挂载脚本、`get_instance_base_type()` 生效、`_ready` 经 `GDVIRTUAL` → `callp()` 派发、内建 API 与绑定层均可调用。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `as_script_language.h/.cpp` | `ScriptLanguage` 实现（单例、语言名 `AngelScript`、扩展名 `as`）。 |
| `as_engine.h/.cpp` | AS 引擎持有者：懒初始化、模块编译、`execute`/`call_function`、内建注册与绑定层接线。 |
| `as_script.h/.cpp` | `Script` 实现（`.as` 资源：基类指令解析、类名校验、编译）。 |
| `as_script_instance.h/.cpp` | `ScriptInstance` 实现（实例创建、属性读写、方法/通知派发）。 |
| `as_resource_format.h/.cpp` | `.as`/`.asb` 的资源加载器与保存器。 |
| `binding/` | M2 绑定层：`as_binding_decl`（内省→声明/编组）、`as_binding_plan`（可见性与计划）、`as_binding_value_types`（内建值类型）、`as_binding_object`（对象类型）、`as_binding_registry`（注册编排）、`as_binding_dumper`（导出产物）。 |
| `register_types.cpp` | 模块注册（`ScriptServer`、资源加载器/保存器、API 导出入口）。 |
| `thirdparty/angelscript/` | AngelScript 2.38.0 官方源码（zlib 许可，见 `LICENSE.txt`）。 |
| `tests/` | doctest 用例；`tests/e2e_project/` 为 headless 验收项目。 |

## 许可

- 第三方 `thirdparty/angelscript/` 使用 zlib 许可，许可全文见 `thirdparty/angelscript/LICENSE.txt`。
- 模块自身源码沿用 Godot 的 MIT 许可头（由 `misc/scripts/copyright_headers.py` 统一维护）。
