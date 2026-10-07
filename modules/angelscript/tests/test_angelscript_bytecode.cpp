/**************************************************************************/
/*  test_angelscript_bytecode.cpp                                         */
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

#include "../as_bytecode.h"
#include "../as_engine.h"
#include "../as_script.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#define ANGELSCRIPT_BYTECODE_TESTS_IMPL
#include "test_angelscript_bytecode.h"

void as_bytecode_container_roundtrip() {
	Vector<StringName> types;
	types.push_back(StringName("Node"));
	types.push_back(StringName("Timer"));

	Vector<uint8_t> payload;
	payload.push_back(0xDE);
	payload.push_back(0xAD);
	payload.push_back(0xBE);
	payload.push_back(0xEF);

	Vector<uint8_t> packed = as_bytecode_pack(StringName("Node"), types, payload);
	REQUIRE(packed.size() > 16);
	CHECK_EQ(packed[0], 0x47); // 'G'
	CHECK_EQ(packed[1], 0x44); // 'D'
	CHECK_EQ(packed[2], 0x41); // 'A'
	CHECK_EQ(packed[3], 0x53); // 'S'
	CHECK_EQ(packed[4], 0x42); // 'B'

	ASByteCode out;
	String err;
	REQUIRE_EQ(as_bytecode_unpack(packed, out, &err), OK);
	CHECK_EQ(out.format_version, AS_BYTECODE_FORMAT_VERSION);
	CHECK(out.base_type == StringName("Node"));
	REQUIRE_EQ(out.required_types.size(), 2);
	CHECK(out.required_types[0] == StringName("Node"));
	CHECK(out.required_types[1] == StringName("Timer"));
	REQUIRE_EQ(out.payload.size(), 4);
	CHECK_EQ(out.payload[0], 0xDE);
	CHECK_EQ(out.payload[3], 0xEF);
}

void as_bytecode_container_rejects_bad_input() {
	Vector<StringName> types;
	types.push_back(StringName("Node"));
	Vector<uint8_t> payload;
	payload.push_back(0x01);
	Vector<uint8_t> good = as_bytecode_pack(StringName("Node"), types, payload);

	ASByteCode out;
	String err;

	Vector<uint8_t> bad_magic = good;
	bad_magic.write[0] = 0x00;
	CHECK_EQ(as_bytecode_unpack(bad_magic, out, &err), ERR_INVALID_DATA);
	CHECK_FALSE(err.is_empty());

	Vector<uint8_t> bad_version = good;
	bad_version.write[8] = 0xFF;
	CHECK_EQ(as_bytecode_unpack(bad_version, out, &err), ERR_INVALID_DATA);

	Vector<uint8_t> truncated = good.slice(0, good.size() - 1);
	CHECK_EQ(as_bytecode_unpack(truncated, out, &err), ERR_INVALID_DATA);

	Vector<uint8_t> tiny;
	tiny.push_back(0x47);
	CHECK_EQ(as_bytecode_unpack(tiny, out, &err), ERR_INVALID_DATA);
}

// 信号在绑定层以 `signal_<name>` 方法约定表示（无 `signal` 关键字），故此处用方法声明。
static const char *AS_BYTECODE_TEST_SOURCE =
		"// godot_base: Node\n"
		"class probe_bytecode {\n"
		"    int counter = 0;\n"
		"    void signal_ping(int value, String label) {}\n"
		"    void _ready() { counter = 1; }\n"
		"    int add(int a, int b) { return a + b; }\n"
		"}\n";

void as_bytecode_save_writes_container() {
	ASEngine::get_singleton()->ensure_initialized();

	Ref<ASScript> script;
	script.instantiate();
	script->set_path("user://asb_save/probe_bytecode.as");
	String error;
	REQUIRE(script->compile_source(AS_BYTECODE_TEST_SOURCE, "user://asb_save/probe_bytecode.as", &error));

	Vector<StringName> types;
	types.push_back(StringName("Node"));
	REQUIRE_EQ(script->save_bytecode("user://asb_save/probe_bytecode.asb", types, &error), OK);

	Error read_error = OK;
	Vector<uint8_t> bytes = FileAccess::get_file_as_bytes("user://asb_save/probe_bytecode.asb", &read_error);
	REQUIRE_EQ(read_error, OK);

	ASByteCode code;
	REQUIRE_EQ(as_bytecode_unpack(bytes, code, &error), OK);
	CHECK(code.base_type == StringName("Node"));
	REQUIRE_EQ(code.required_types.size(), 1);
	CHECK(code.required_types[0] == StringName("Node"));
	CHECK(code.payload.size() > 0);
}

void as_bytecode_load_matches_source_introspection() {
	ASEngine::get_singleton()->ensure_initialized();

	const String source_path = "user://asb_pair_src/probe_bytecode.as";
	Ref<ASScript> source_script;
	source_script.instantiate();
	source_script->set_path(source_path);
	String error;
	REQUIRE(source_script->compile_source(AS_BYTECODE_TEST_SOURCE, source_path, &error));

	Vector<StringName> types;
	types.push_back(StringName("Node"));
	REQUIRE_EQ(source_script->save_bytecode("user://asb_pair_src/probe_bytecode.asb", types, &error), OK);

	Error read_error = OK;
	Vector<uint8_t> bytes = FileAccess::get_file_as_bytes("user://asb_pair_src/probe_bytecode.asb", &read_error);
	REQUIRE_EQ(read_error, OK);

	// 字节码脚本的类名必须等于文件名，故与源码脚本使用相同的 basename。
	Ref<ASScript> binary_script;
	binary_script.instantiate();
	CAPTURE(error);
	REQUIRE(binary_script->load_bytecode(bytes, "user://asb_pair_bin/probe_bytecode.as", &error));

	CHECK(binary_script->is_valid());
	CHECK_EQ(binary_script->get_instance_base_type(), StringName("Node"));
	CHECK(binary_script->has_method(StringName("_ready")));
	CHECK(binary_script->has_method(StringName("add")));
	CHECK_FALSE(binary_script->has_method(StringName("signal_ping"))); // signal_ 前缀方法不计入普通方法表。

	// 方法/属性内省在源码态与字节码态必须逐项一致（数量与内容均来自 asITypeInfo）。
	List<MethodInfo> source_methods;
	source_script->get_script_method_list(&source_methods);
	List<MethodInfo> binary_methods;
	binary_script->get_script_method_list(&binary_methods);
	CHECK_EQ(source_methods.size(), binary_methods.size());

	List<PropertyInfo> source_props;
	source_script->get_script_property_list(&source_props);
	List<PropertyInfo> binary_props;
	binary_script->get_script_property_list(&binary_props);
	CHECK_EQ(source_props.size(), binary_props.size());

	// 信号及参数名依赖保存时的调试信息（stripDebugInfo=false）。
	CHECK(source_script->has_script_signal(StringName("ping")));
	CHECK(binary_script->has_script_signal(StringName("ping")));

	List<MethodInfo> binary_signals;
	binary_script->get_script_signal_list(&binary_signals);
	bool found_ping = false;
	for (List<MethodInfo>::Element *E = binary_signals.front(); E; E = E->next()) {
		const MethodInfo &mi = E->get();
		if (mi.name != StringName("ping")) {
			continue;
		}
		found_ping = true;
		REQUIRE_EQ(mi.arguments.size(), 2);
		CHECK_EQ(mi.arguments[0].name, StringName("value"));
		CHECK_EQ(mi.arguments[1].name, StringName("label"));
	}
	CHECK(found_ping);
}

void as_bytecode_load_rejects_corrupted_payload() {
	ASEngine::get_singleton()->ensure_initialized();

	const String source_path = "user://asb_corrupt_src/probe_bytecode.as";
	Ref<ASScript> source_script;
	source_script.instantiate();
	source_script->set_path(source_path);
	String error;
	REQUIRE(source_script->compile_source(AS_BYTECODE_TEST_SOURCE, source_path, &error));

	Vector<StringName> types;
	types.push_back(StringName("Node"));
	REQUIRE_EQ(source_script->save_bytecode("user://asb_corrupt_src/probe_bytecode.asb", types, &error), OK);

	Error read_error = OK;
	Vector<uint8_t> bytes = FileAccess::get_file_as_bytes("user://asb_corrupt_src/probe_bytecode.asb", &read_error);
	REQUIRE_EQ(read_error, OK);

	ASByteCode code;
	REQUIRE_EQ(as_bytecode_unpack(bytes, code, &error), OK);
	// 保留合法的容器头与符号表，只把 payload 截断到 4 字节，加载必然失败。
	Vector<uint8_t> corrupted = bytes.slice(0, bytes.size() - code.payload.size() + 4);

	Ref<ASScript> binary_script;
	binary_script.instantiate();
	String load_error;
	CHECK_FALSE(binary_script->load_bytecode(corrupted, "user://asb_corrupt_bin/probe_bytecode.as", &load_error));
	CHECK_FALSE(load_error.is_empty());
	CHECK_FALSE(binary_script->is_valid());

	// 失败后不得污染引擎：仍能正常编译源码脚本（另用不同路径，避免与 source_script 争用资源缓存）。
	Ref<ASScript> after_script;
	after_script.instantiate();
	const String after_path = "user://asb_corrupt_after/probe_bytecode.as";
	after_script->set_path(after_path);
	CHECK(after_script->compile_source(AS_BYTECODE_TEST_SOURCE, after_path, &error));
}
