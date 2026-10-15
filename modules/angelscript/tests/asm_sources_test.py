#!/usr/bin/env python
"""asm_sources.select_callfunc_sources 的选择表自检。

运行：python modules/angelscript/tests/asm_sources_test.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import asm_sources  # noqa: E402


def check(platform, arch, msvc, asm, portability):
    sel = asm_sources.select_callfunc_sources(platform, arch, msvc)
    assert sel.asm_sources == asm, (platform, arch, msvc, sel.asm_sources)
    assert sel.max_portability == portability, (platform, arch, msvc, sel.max_portability)


# x86_64
check("windows", "x86_64", True, ["as_callfunc_x64_msvc_asm.asm"], False)
check("linuxbsd", "x86_64", False, [], False)
check("macos", "x86_64", False, [], False)
check("windows", "x86_64", False, [], False)  # MinGW

# arm64
check("macos", "arm64", False, ["as_callfunc_arm64_xcode.S"], False)
check("ios", "arm64", False, ["as_callfunc_arm64_xcode.S"], False)
check("visionos", "arm64", False, ["as_callfunc_arm64_xcode.S"], False)
check("linuxbsd", "arm64", False, ["as_callfunc_arm64_gcc.S"], False)
check("android", "arm64", False, ["as_callfunc_arm64_gcc.S"], False)
check("windows", "arm64", True, [], True)  # Windows arm64 降级

# arm32
check("macos", "arm32", False, ["as_callfunc_arm_xcode.S"], False)
check("android", "arm32", False, ["as_callfunc_arm_gcc.S"], False)

# riscv64
check("linuxbsd", "riscv64", False, ["as_callfunc_riscv64_gcc.S"], False)

# 未映射 → 可移植降级
check("linuxbsd", "ppc64", False, [], True)
check("web", "wasm32", False, [], True)
check("linuxbsd", "x86_32", False, [], True)

print("asm_sources: all checks passed")
