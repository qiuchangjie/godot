/**************************************************************************/
/*  test_angelscript_lazy_binding.cpp                                     */
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

#include "../binding/as_binding_scanner.h"

#define ANGELSCRIPT_LAZY_BINDING_TESTS_IMPL
#include "test_angelscript_lazy_binding.h"

void as_lazy_scanner_intersects_classdb() {
	HashSet<StringName> types;
	ASBindingScanner::scan_source("Node n; Sprite2D s; NotARealClass x;", types);
	CHECK(types.has(StringName("Node")));
	CHECK(types.has(StringName("Sprite2D")));
	CHECK(!types.has(StringName("NotARealClass"))); // 与 ClassDB 求交后丢弃。
}

void as_lazy_scanner_ignores_comments_and_strings() {
	HashSet<StringName> types;
	ASBindingScanner::scan_source(
			"// Node is just a comment\n"
			"const String s = \"Sprite2D inside a string\";\n"
			"Control c;\n",
			types);
	CHECK(types.has(StringName("Control")));
	CHECK(!types.has(StringName("Node")));
	CHECK(!types.has(StringName("Sprite2D")));
}
