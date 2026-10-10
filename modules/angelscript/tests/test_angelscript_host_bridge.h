/**************************************************************************/
/*  test_angelscript_host_bridge.h                                        */
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

// 用例注册头：不得 include 任何 AngelScript 头。实现见 test_angelscript_host_bridge.cpp。
void as_host_bridge_install_validation();
void as_host_bridge_invoke_dispatch();
void as_host_bridge_stale_object_is_safe();
void as_host_call_scalar_roundtrip();
void as_host_call_string_and_container_roundtrip();
void as_host_call_without_table_reports_error();
void as_host_call_bidirectional_roundtrip();
void as_host_call_object_roundtrip_and_lifetime();
void as_host_call_empty_array_reaches_host();
void as_host_call_stale_object_reaches_host();

#ifndef ANGELSCRIPT_HOST_BRIDGE_TESTS_IMPL

TEST_CASE("[AngelScript][HostBridge] install validates abi version and struct size") {
	as_host_bridge_install_validation();
}

TEST_CASE("[AngelScript][HostBridge] invoke dispatches to the installed table") {
	as_host_bridge_invoke_dispatch();
}

TEST_CASE("[AngelScript][HostBridge] stale object argument is passed safely") {
	as_host_bridge_stale_object_is_safe();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call scalar roundtrip") {
	as_host_call_scalar_roundtrip();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call string and container roundtrip") {
	as_host_call_string_and_container_roundtrip();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call without a table reports an error") {
	as_host_call_without_table_reports_error();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call bidirectional roundtrip") {
	as_host_call_bidirectional_roundtrip();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call object roundtrip and lifetime") {
	as_host_call_object_roundtrip_and_lifetime();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call forwards an empty Array as zero args") {
	as_host_call_empty_array_reaches_host();
}

TEST_CASE("[AngelScript][HostBridge] as_host_call forwards a stale object safely") {
	as_host_call_stale_object_reaches_host();
}

#endif // ANGELSCRIPT_HOST_BRIDGE_TESTS_IMPL
