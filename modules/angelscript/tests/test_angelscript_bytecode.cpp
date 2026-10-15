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
#include "../as_resource_format.h"
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

void as_bytecode_stream_read_stops_at_end() {
	const uint8_t data[3] = { 0xAA, 0xBB, 0xCC };
	ASMemoryReadStream stream(data, 3);

	uint8_t buf[4] = { 0 };
	REQUIRE_EQ(stream.Read(buf, 3), 3);
	CHECK_EQ(buf[0], 0xAA);
	CHECK_EQ(buf[1], 0xBB);
	CHECK_EQ(buf[2], 0xCC);

	// 读尽后任意读取必须返回负数：asCReader::ReadData 只把负返回当 EOF，
	// 返回 0 会被当作「成功读到 0 字节」而留下未初始化目标内存。
	CHECK(stream.Read(buf, 1) < 0);
	CHECK(stream.Read(buf, 4) < 0);
	// 零长度读不算越界，也不推进游标。
	CHECK_EQ(stream.Read(buf, 0), 0);
	CHECK(stream.Read(buf, 1) < 0);
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

static Vector<MethodInfo> _methods_sorted_by_name(const List<MethodInfo> &p_in) {
	Vector<MethodInfo> out;
	for (const MethodInfo &mi : p_in) {
		out.push_back(mi);
	}
	// 简单插入排序：内省顺序无保证，需按名字归一后逐项比较。
	for (int i = 1; i < out.size(); i++) {
		MethodInfo key = out[i];
		int j = i - 1;
		while (j >= 0 && out[j].name > key.name) {
			out.set(j + 1, out[j]);
			j--;
		}
		out.set(j + 1, key);
	}
	return out;
}

static Vector<PropertyInfo> _properties_sorted_by_name(const List<PropertyInfo> &p_in) {
	Vector<PropertyInfo> out;
	for (const PropertyInfo &pi : p_in) {
		out.push_back(pi);
	}
	for (int i = 1; i < out.size(); i++) {
		PropertyInfo key = out[i];
		int j = i - 1;
		while (j >= 0 && out[j].name > key.name) {
			out.set(j + 1, out[j]);
			j--;
		}
		out.set(j + 1, key);
	}
	return out;
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

	CHECK(binary_script->is_script_valid());
	CHECK_EQ(binary_script->get_instance_base_type(), StringName("Node"));
	CHECK(binary_script->has_method(StringName("_ready")));
	CHECK(binary_script->has_method(StringName("add")));
	CHECK_FALSE(binary_script->has_method(StringName("signal_ping"))); // signal_ 前缀方法不计入普通方法表。

	// 方法/属性内省在源码态与字节码态必须逐项一致（数量与内容均来自 asITypeInfo）。
	List<MethodInfo> source_methods_list;
	source_script->get_script_method_list(&source_methods_list);
	List<MethodInfo> binary_methods_list;
	binary_script->get_script_method_list(&binary_methods_list);
	Vector<MethodInfo> source_methods = _methods_sorted_by_name(source_methods_list);
	Vector<MethodInfo> binary_methods = _methods_sorted_by_name(binary_methods_list);
	REQUIRE_EQ(source_methods.size(), binary_methods.size());
	for (int i = 0; i < source_methods.size(); i++) {
		CHECK_EQ(source_methods[i].name, binary_methods[i].name);
		CHECK_EQ(source_methods[i].return_val.type, binary_methods[i].return_val.type);
		REQUIRE_EQ(source_methods[i].arguments.size(), binary_methods[i].arguments.size());
		for (int j = 0; j < source_methods[i].arguments.size(); j++) {
			CHECK_EQ(source_methods[i].arguments[j].name, binary_methods[i].arguments[j].name);
			CHECK_EQ(source_methods[i].arguments[j].type, binary_methods[i].arguments[j].type);
		}
	}

	List<PropertyInfo> source_props_list;
	source_script->get_script_property_list(&source_props_list);
	List<PropertyInfo> binary_props_list;
	binary_script->get_script_property_list(&binary_props_list);
	Vector<PropertyInfo> source_props = _properties_sorted_by_name(source_props_list);
	Vector<PropertyInfo> binary_props = _properties_sorted_by_name(binary_props_list);
	REQUIRE_EQ(source_props.size(), binary_props.size());
	for (int i = 0; i < source_props.size(); i++) {
		CHECK_EQ(source_props[i].name, binary_props[i].name);
		CHECK_EQ(source_props[i].type, binary_props[i].type);
	}

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
	const int payload_start = bytes.size() - code.payload.size();

	// 保留合法的容器头与符号表，把 payload 截断到不同偏移，加载都必须失败
	// （越界读取必须报 EOF，而不是把未初始化内存喂给反序列化器）。
	const float ratios[4] = { 0.0f, 0.1f, 0.5f, 0.9f };
	for (int i = 0; i < 4; i++) {
		int keep_payload = (int)((float)code.payload.size() * ratios[i]);
		if (keep_payload < 1) {
			keep_payload = 1;
		}
		Vector<uint8_t> corrupted = bytes.slice(0, payload_start + keep_payload);

		Ref<ASScript> binary_script;
		binary_script.instantiate();
		String load_error;
		CHECK_FALSE(binary_script->load_bytecode(corrupted, vformat("user://asb_corrupt_bin/probe_bytecode_%d.as", i), &load_error));
		CHECK_FALSE(load_error.is_empty());
		CHECK_FALSE(binary_script->is_script_valid());
	}

	// 失败后不得污染引擎：仍能正常编译源码脚本（另用不同路径，避免与 source_script 争用资源缓存）。
	Ref<ASScript> after_script;
	after_script.instantiate();
	const String after_path = "user://asb_corrupt_after/probe_bytecode.as";
	after_script->set_path(after_path);
	CHECK(after_script->compile_source(AS_BYTECODE_TEST_SOURCE, after_path, &error));
}

void resource_loader_loads_asb() {
	ASEngine::get_singleton()->ensure_initialized();

	Ref<ASScript> script;
	script.instantiate();
	String error;
	REQUIRE(script->compile_source(AS_BYTECODE_TEST_SOURCE, "user://asb_loader/probe_bytecode.as", &error));

	Vector<StringName> types;
	types.push_back(StringName("Node"));
	REQUIRE_EQ(script->save_bytecode("user://asb_loader/probe_bytecode.asb", types, &error), OK);

	// 直接实例化加载器并禁用缓存，验证 `.asb` 分支本身而非 ResourceLoader 的缓存行为。
	Ref<ASResourceFormatLoaderASScript> loader;
	loader.instantiate();
	Error load_error = OK;
	Ref<Resource> resource = loader->load("user://asb_loader/probe_bytecode.asb", "", &load_error, false, nullptr, ResourceFormatLoader::CACHE_MODE_IGNORE);
	REQUIRE_EQ(load_error, OK);
	REQUIRE(resource.is_valid());

	Ref<ASScript> loaded_script = resource;
	REQUIRE(loaded_script.is_valid());
	CHECK(loaded_script->is_script_valid());
	CHECK_EQ(loaded_script->get_instance_base_type(), StringName("Node"));
}

void compile_script_produces_loadable_bytecode() {
	ASEngine::get_singleton()->ensure_initialized();

	// 先写一个真实源码文件，模拟工程内 `.as`。
	DirAccess::make_dir_recursive_absolute("user://asb_compile");
	{
		Ref<FileAccess> src = FileAccess::open("user://asb_compile/probe_bytecode.as", FileAccess::WRITE);
		REQUIRE(src.is_valid());
		src->store_string(AS_BYTECODE_TEST_SOURCE);
	}

	String error;
	REQUIRE_EQ(as_bytecode_compile_script("user://asb_compile/probe_bytecode.as", "user://asb_compile/probe_bytecode.asb", &error), OK);

	Error read_error = OK;
	Vector<uint8_t> bytes = FileAccess::get_file_as_bytes("user://asb_compile/probe_bytecode.asb", &read_error);
	REQUIRE_EQ(read_error, OK);

	ASByteCode code;
	REQUIRE_EQ(as_bytecode_unpack(bytes, code, &error), OK);
	// 符号表应包含扫描到的类 + 基类。
	CHECK(code.required_types.size() >= 1);
	bool has_node = false;
	for (int i = 0; i < code.required_types.size(); i++) {
		if (code.required_types[i] == StringName("Node")) {
			has_node = true;
		}
	}
	CHECK(has_node);

	Ref<ASScript> loaded;
	loaded.instantiate();
	REQUIRE(loaded->load_bytecode(bytes, "user://asb_compile_bin/probe_bytecode.as", &error));
	CHECK(loaded->is_script_valid());
}

void as_bytecode_rejects_stale_format_version() {
	// 原生值存储（P3）提升了容器布局版本；任何早于当前版本的 `.asb` 都必须被显式拒绝，
	// 而不是被静默按新布局反序列化。用 `FORMAT_VERSION - 1` 表达「上一版」，使该断言
	// 随版本提升自动保持「拒绝前一版布局」的语义。
	const uint32_t stale_version = AS_BYTECODE_FORMAT_VERSION - 1;

	Vector<StringName> types;
	types.push_back(StringName("Node"));
	Vector<uint8_t> payload;
	payload.push_back(0x01);
	Vector<uint8_t> bytes = as_bytecode_pack(StringName("Node"), types, payload);
	REQUIRE(bytes.size() >= 16);

	// 容器头偏移 8..11 为小端 u32 的 format_version，改写为旧版本。
	bytes.write[8] = (uint8_t)(stale_version & 0xFF);
	bytes.write[9] = (uint8_t)((stale_version >> 8) & 0xFF);
	bytes.write[10] = (uint8_t)((stale_version >> 16) & 0xFF);
	bytes.write[11] = (uint8_t)((stale_version >> 24) & 0xFF);

	ASByteCode out;
	String err;
	CHECK_EQ(as_bytecode_unpack(bytes, out, &err), ERR_INVALID_DATA);
	CHECK_FALSE(err.is_empty());
}
