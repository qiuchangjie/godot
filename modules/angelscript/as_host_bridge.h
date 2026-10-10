/**************************************************************************/
/*  as_host_bridge.h                                                      */
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

#include "core/error/error_list.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"

// M5 互操作 L2：版本化的宿主回调表契约（spec §4）。
// 任何嵌入方（生产环境的 C# 绑定层、测试桩）按同一布局构造并 install()。
#define AS_HOST_BRIDGE_ABI_VERSION 1u

struct ASHostCallbacks {
	uint32_t abi_version = 0;
	uint32_t struct_size = 0;
	void *user_data = nullptr;
	// AS->宿主：按 method_id 调用宿主方法。args/argc 为输入，r_ret 为输出。
	// 返回 0 成功，非 0 失败（as_host_call 据此返回空 Variant + 诊断）。
	int (*invoke)(void *user_data, int32_t method_id, const Variant *args, int32_t argc, Variant *r_ret) = nullptr;
};

// 宿主桥单例（非 Godot Object）：持有并校验回调表、按 method_id 派发。
// 主线程独占；本类不做线程同步（spec §6.5）。
class ASHostBridge {
	ASHostCallbacks callbacks;
	bool installed = false;

public:
	static ASHostBridge *get_singleton();

	// 校验 abi_version / struct_size / invoke 后拷贝入表；非法即拒绝且不改变原状态。
	Error install(const ASHostCallbacks *p_callbacks);
	void uninstall();
	bool is_installed() const { return installed; }

	// p_args 可为 nullptr（当 p_argc == 0）。未安装返回 ERR_UNCONFIGURED。
	Error invoke(int p_method_id, const Variant *p_args, int p_argc, Variant *r_ret);
};
