# AngelScript 平台汇编源选择。
#
# 依据目标平台/架构/编译器返回需要显式接线到构建系统的汇编源文件名
# （相对 modules/angelscript/thirdparty/angelscript/source/）。
#
# 说明：x86_64 的 GCC/Clang 实现完整内联于 as_callfunc_x64_gcc.cpp，无需外部汇编；
# 未映射的架构回退到 AngelScript 的 AS_MAX_PORTABILITY 纯可移植实现。

from dataclasses import dataclass, field


@dataclass
class CallfuncSelection:
    asm_sources: list = field(default_factory=list)
    max_portability: bool = False


_APPLE_PLATFORMS = {"macos", "ios", "visionos"}


def select_callfunc_sources(platform, arch, msvc):
    is_apple = platform in _APPLE_PLATFORMS

    if arch == "x86_64":
        if msvc:
            return CallfuncSelection(["as_callfunc_x64_msvc_asm.asm"], False)
        return CallfuncSelection([], False)

    if arch == "arm64":
        if is_apple:
            return CallfuncSelection(["as_callfunc_arm64_xcode.S"], False)
        if msvc:
            # Windows arm64 暂无可用汇编实现，走可移植降级。
            return CallfuncSelection([], True)
        return CallfuncSelection(["as_callfunc_arm64_gcc.S"], False)

    if arch in ("arm32", "arm"):
        if is_apple:
            return CallfuncSelection(["as_callfunc_arm_xcode.S"], False)
        return CallfuncSelection(["as_callfunc_arm_gcc.S"], False)

    if arch == "riscv64":
        return CallfuncSelection(["as_callfunc_riscv64_gcc.S"], False)

    return CallfuncSelection([], True)
