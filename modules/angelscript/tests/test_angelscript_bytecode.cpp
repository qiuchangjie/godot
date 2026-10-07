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

	Vector<uint8_t> packed = as_bytecode_pack(types, payload);
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
	Vector<uint8_t> good = as_bytecode_pack(types, payload);

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
