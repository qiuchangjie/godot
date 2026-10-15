@echo off
chcp 65001 >nul
REM ===========================================================================
REM  build-both.cmd
REM  构建「同时支持 C# (mono) 与 AngelScript」的 Godot 引擎。
REM
REM  背景：
REM    - AngelScript 模块恒开（modules/angelscript/config.py 的 can_build
REM      无条件返回 True），无需任何 scons 开关；
REM    - C# 由 module_mono_enabled 控制，默认关闭，必须显式 =yes；
REM    - mono 还需按官方流程生成 glue 与 GodotSharp 程序集
REM      （见 modules/mono/README.md）。
REM
REM  步骤：
REM    1/5 加载 vcvars64（提供 ml64，AngelScript 的 x64 汇编需要）
REM    2/5 构建 editor（C# + AngelScript）
REM    3/5 生成 mono glue
REM    4/5 构建 GodotSharp / Godot.NET.Sdk 程序集（需要 .NET SDK）
REM    5/5 构建导出模板 template_debug + template_release（可关闭）
REM
REM  用法：
REM    build-both.cmd                 使用下方默认开关
REM    build-both.cmd deprecated=no   额外参数透传给每次 scons 调用
REM    set VCVARS=<path>\vcvars64.bat 覆盖自动探测到的 vcvars64 路径
REM
REM  说明：
REM    - 本模块只支持 64 位，编辑器二进制固定为 x86_64；
REM    - 若 step 4 报 NuGet 版本冲突，按 modules/mono/README.md 建本地源后
REM      追加 --push-nupkgs-local <本地源> 重跑。
REM ===========================================================================

setlocal
pushd "%~dp0"

REM ---- 可调开关 -------------------------------------------------------------
set "BUILD_TEMPLATES=1"
set "RUN_TESTS=1"
set "JOBS=8"
set "PLATFORM=windows"
REM 编辑器二进制（本模块只支持 64 位，故固定 x86_64）
set "GODOT_BIN=bin\godot.windows.editor.x86_64.mono.console.exe"
REM --------------------------------------------------------------------------

set "EDITOR_ARGS=platform=%PLATFORM% accesskit=no d3d12=no -j%JOBS% module_mono_enabled=yes"
if "%RUN_TESTS%"=="1" set "EDITOR_ARGS=%EDITOR_ARGS% tests=yes"
set "TEMPLATE_ARGS=platform=%PLATFORM% accesskit=no d3d12=no -j%JOBS% module_mono_enabled=yes"

REM ---- 1/5 vcvars64 ---------------------------------------------------------
set "VCVARS_PATH="
if defined VCVARS set "VCVARS_PATH=%VCVARS%"
if not defined VCVARS_PATH (
	for %%E in (Community Professional Enterprise BuildTools) do (
		if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
			set "VCVARS_PATH=C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
		)
	)
)
if not defined VCVARS_PATH (
	echo [build-both] 找不到 vcvars64.bat，请设置环境变量 VCVARS 指向它。
	goto :fail
)
echo [build-both] 1/5 加载 vcvars64: %VCVARS_PATH%
call "%VCVARS_PATH%" >nul
if errorlevel 1 (
	echo [build-both] vcvars64.bat 执行失败。
	goto :fail
)

REM ---- 2/5 editor -----------------------------------------------------------
echo.
echo [build-both] 2/5 构建编辑器（C# + AngelScript）...
scons target=editor %EDITOR_ARGS% %*
if errorlevel 1 goto :fail

REM ---- 3/5 mono glue --------------------------------------------------------
echo.
echo [build-both] 3/5 生成 mono glue...
if not exist "%GODOT_BIN%" (
	echo [build-both] 未找到编辑器二进制 "%GODOT_BIN%"。
	goto :fail
)
"%GODOT_BIN%" --headless --generate-mono-glue modules/mono/glue
if errorlevel 1 goto :fail

REM ---- 4/5 GodotSharp 程序集 ------------------------------------------------
echo.
echo [build-both] 4/5 构建 GodotSharp 程序集...
python modules\mono\build_scripts\build_assemblies.py --godot-output-dir .\bin --godot-platform %PLATFORM%
if errorlevel 1 goto :fail

REM ---- 5/5 导出模板 ---------------------------------------------------------
if "%BUILD_TEMPLATES%"=="1" (
	echo.
	echo [build-both] 5/5 构建导出模板 template_debug...
	scons target=template_debug %TEMPLATE_ARGS% %*
	if errorlevel 1 goto :fail
	echo.
	echo [build-both] 5/5 构建导出模板 template_release...
	scons target=template_release %TEMPLATE_ARGS% %*
	if errorlevel 1 goto :fail
) else (
	echo.
	echo [build-both] 已跳过导出模板（BUILD_TEMPLATES=0）。
)

echo.
echo [build-both] 完成：编辑器与（可选）导出模板均支持 C# + AngelScript。
popd
endlocal
exit /b 0

:fail
echo.
echo [build-both] 构建失败。
popd
endlocal
exit /b 1
