/**************************************************************************/
/*  as_bytecode.h                                                         */
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

#ifndef AS_BYTECODE_H
#define AS_BYTECODE_H

#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"

#include <angelscript.h>

// `.asb` 容器头版本。仅描述容器本身，与 AngelScript 模块 ABI 版本无关
// （ABI/签名/回退策略由业务层 manifest 负责）。
constexpr uint32_t AS_BYTECODE_FORMAT_VERSION = 1;

struct ASByteCode {
	uint32_t format_version = 0;
	Vector<StringName> required_types;
	Vector<uint8_t> payload;
};

Vector<uint8_t> as_bytecode_pack(const Vector<StringName> &p_required_types, const Vector<uint8_t> &p_payload);
Error as_bytecode_unpack(const Vector<uint8_t> &p_bytes, ASByteCode &r_out, String *r_error = nullptr);

// asIBinaryStream 适配器：AngelScript 只要求 Read/Write 两个方法。
class ASMemoryReadStream : public asIBinaryStream {
	const uint8_t *data = nullptr;
	uint32_t size = 0;
	uint32_t pos = 0;

public:
	ASMemoryReadStream(const uint8_t *p_data, uint32_t p_size);
	int Read(void *ptr, asUINT size) override;
	int Write(const void *ptr, asUINT size) override;
};

class ASVectorWriteStream : public asIBinaryStream {
	Vector<uint8_t> &buffer;

public:
	explicit ASVectorWriteStream(Vector<uint8_t> &p_buffer);
	int Read(void *ptr, asUINT size) override;
	int Write(const void *ptr, asUINT size) override;
};

#endif // AS_BYTECODE_H
