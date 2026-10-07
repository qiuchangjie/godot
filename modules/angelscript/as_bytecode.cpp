/**************************************************************************/
/*  as_bytecode.cpp                                                       */
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

#include "as_bytecode.h"

#include "as_engine.h"
#include "as_script.h"

#include "binding/as_binding_scanner.h"

#include "core/io/file_access.h"

static const uint8_t AS_BYTECODE_MAGIC[8] = { 'G', 'D', 'A', 'S', 'B', 0, 0, 0 };
// 容器自检上限，防止畸形文件触发超大分配。
static const uint32_t AS_BYTECODE_MAX_TYPES = 1 << 16;
static const uint32_t AS_BYTECODE_MAX_TYPE_NAME = 1 << 10;

static void _append_u32_le(Vector<uint8_t> &r_out, uint32_t p_value) {
	r_out.push_back((uint8_t)(p_value & 0xFF));
	r_out.push_back((uint8_t)((p_value >> 8) & 0xFF));
	r_out.push_back((uint8_t)((p_value >> 16) & 0xFF));
	r_out.push_back((uint8_t)((p_value >> 24) & 0xFF));
}

static uint32_t _read_u32_le(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

Vector<uint8_t> as_bytecode_pack(const StringName &p_base_type, const Vector<StringName> &p_required_types, const Vector<uint8_t> &p_payload) {
	Vector<uint8_t> out;
	out.resize(8);
	memcpy(out.ptrw(), AS_BYTECODE_MAGIC, 8);
	_append_u32_le(out, AS_BYTECODE_FORMAT_VERSION);
	CharString base_utf8 = String(p_base_type).utf8();
	_append_u32_le(out, (uint32_t)base_utf8.length());
	for (int j = 0; j < base_utf8.length(); j++) {
		out.push_back((uint8_t)base_utf8[j]);
	}
	_append_u32_le(out, (uint32_t)p_required_types.size());
	for (int i = 0; i < p_required_types.size(); i++) {
		CharString utf8 = String(p_required_types[i]).utf8();
		_append_u32_le(out, (uint32_t)utf8.length());
		for (int j = 0; j < utf8.length(); j++) {
			out.push_back((uint8_t)utf8[j]);
		}
	}
	out.append_array(p_payload);
	return out;
}

Error as_bytecode_unpack(const Vector<uint8_t> &p_bytes, ASByteCode &r_out, String *r_error) {
	const uint8_t *data = p_bytes.ptr();
	int size = p_bytes.size();
	auto fail = [r_error](const char *p_msg) -> Error {
		if (r_error) {
			*r_error = p_msg;
		}
		return ERR_INVALID_DATA;
	};

	if (size < 16) {
		return fail("AngelScript bytecode is too small to contain a container header.");
	}
	for (int i = 0; i < 8; i++) {
		if (data[i] != AS_BYTECODE_MAGIC[i]) {
			return fail("AngelScript bytecode container has an invalid magic.");
		}
	}
	uint32_t version = _read_u32_le(data + 8);
	if (version != AS_BYTECODE_FORMAT_VERSION) {
		return fail("AngelScript bytecode container uses an unsupported format version.");
	}

	int offset = 12;
	uint32_t base_len = _read_u32_le(data + offset);
	offset += 4;
	if (base_len > AS_BYTECODE_MAX_TYPE_NAME || offset + (int)base_len > size) {
		return fail("AngelScript bytecode container is truncated in the base type name.");
	}
	String base_name = String::utf8((const char *)data + offset, (int)base_len);
	offset += (int)base_len;

	if (offset + 4 > size) {
		return fail("AngelScript bytecode container is truncated in the type count.");
	}
	uint32_t count = _read_u32_le(data + offset);
	offset += 4;
	if (count > AS_BYTECODE_MAX_TYPES) {
		return fail("AngelScript bytecode container declares too many required types.");
	}

	Vector<StringName> types;
	types.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		if (offset + 4 > size) {
			return fail("AngelScript bytecode container is truncated in a type length.");
		}
		uint32_t name_len = _read_u32_le(data + offset);
		offset += 4;
		if (name_len > AS_BYTECODE_MAX_TYPE_NAME || offset + (int)name_len > size) {
			return fail("AngelScript bytecode container is truncated in a type name.");
		}
		String name = String::utf8((const char *)data + offset, (int)name_len);
		offset += (int)name_len;
		types.set(i, StringName(name));
	}
	if (offset >= size) {
		return fail("AngelScript bytecode container has an empty payload.");
	}

	r_out.format_version = version;
	r_out.base_type = StringName(base_name);
	r_out.required_types = types;
	r_out.payload = p_bytes.slice(offset, size);
	if (r_error) {
		*r_error = String();
	}
	return OK;
}

ASMemoryReadStream::ASMemoryReadStream(const uint8_t *p_data, uint32_t p_size) :
		data(p_data), size(p_size) {}

int ASMemoryReadStream::Read(void *ptr, asUINT p_size) {
	// AngelScript 的 asCReader::ReadData 只把**负**返回当 EOF；返回 0 会被当成
	// 「成功读到 0 字节」，从而把未初始化的目标内存喂给反序列化器（UB）。
	if (p_size == 0) {
		return 0;
	}
	if (pos > size || p_size > size - pos) {
		return -1;
	}
	memcpy(ptr, data + pos, p_size);
	pos += p_size;
	return (int)p_size;
}

int ASMemoryReadStream::Write(const void *p_ptr, asUINT p_size) {
	return -1;
}

ASVectorWriteStream::ASVectorWriteStream(Vector<uint8_t> &p_buffer) :
		buffer(p_buffer) {}

int ASVectorWriteStream::Read(void *p_ptr, asUINT p_size) {
	return -1;
}

int ASVectorWriteStream::Write(const void *p_ptr, asUINT p_size) {
	int old_size = buffer.size();
	buffer.resize(old_size + (int)p_size);
	if (p_size > 0) {
		memcpy(buffer.ptrw() + old_size, p_ptr, p_size);
	}
	return (int)p_size;
}

Error as_bytecode_compile_script(const String &p_source_path, const String &p_output_path, String *r_error) {
	Error read_error = OK;
	String source = FileAccess::get_file_as_string(p_source_path, &read_error);
	if (read_error != OK) {
		if (r_error) {
			*r_error = vformat("Cannot read AngelScript source '%s'.", p_source_path);
		}
		return read_error;
	}

	Ref<ASScript> script;
	script.instantiate();
	script->set_path(p_source_path);

	String compile_error;
	if (!script->compile_source(source, p_source_path, &compile_error)) {
		if (r_error) {
			*r_error = compile_error;
		}
		return ERR_PARSE_ERROR;
	}

	// 扫描器忽略注释与字符串，`// godot_base:` 里的基类不会被扫到，需显式并入。
	HashSet<StringName> scanned;
	ASBindingScanner::scan_source(source, scanned);
	scanned.insert(script->get_instance_base_type());

	Vector<StringName> required_types;
	required_types.resize(scanned.size());
	int index = 0;
	for (const StringName &type : scanned) {
		required_types.set(index++, type);
	}

	return script->save_bytecode(p_output_path, required_types, r_error);
}
