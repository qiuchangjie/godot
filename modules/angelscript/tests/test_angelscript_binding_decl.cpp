/**************************************************************************/
/*  test_angelscript_binding_decl.cpp                                     */
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

#include "../binding/as_binding_decl.h"

// 必须先定义该宏再包含测试头：本文件与 tests/test_main.cpp（经 modules_tests.gen.h）
// 都会展开该头，不定义宏就会把 TEST_CASE 注册两遍、用例被跑两次。
#define ANGELSCRIPT_BINDING_DECL_TESTS_IMPL
#include "test_angelscript_binding_decl.h"

// 本仓 doctest 无异常：每个前置条件 REQUIRE 后都显式 return 守卫，避免退化时空指针崩溃。
static PropertyInfo make_pi(Variant::Type p_type, const StringName &p_class = StringName(), PropertyHint p_hint = PROPERTY_HINT_NONE) {
	PropertyInfo pi;
	pi.type = p_type;
	pi.class_name = p_class;
	pi.hint = p_hint;
	return pi;
}

void as_binding_decl_scalar_mapping() {
	CHECK(ASBindingDecl::variant_type_to_as(Variant::BOOL) == "bool");
	CHECK(ASBindingDecl::variant_type_to_as(Variant::INT) == "int64"); // 64 位，不能退化为 int
	CHECK(ASBindingDecl::variant_type_to_as(Variant::FLOAT) == "double");
	CHECK(ASBindingDecl::variant_type_to_as(Variant::STRING) == "String");
	CHECK(ASBindingDecl::variant_type_to_as(Variant::VECTOR2) == "Vector2");
	CHECK(ASBindingDecl::variant_type_to_as(Variant::NIL).is_empty());
}

void as_binding_decl_object_and_enum() {
	ASBindingType o = ASBindingDecl::resolve(make_pi(Variant::OBJECT, "Node"));
	REQUIRE(o.valid);
	if (!o.valid) {
		return;
	}
	CHECK(o.as_name == "Node@");

	ASBindingType bare = ASBindingDecl::resolve(make_pi(Variant::OBJECT));
	CHECK(bare.valid);
	CHECK(bare.as_name == "Object@");

	// 枚举型属性 M2 退化为整数（spec §3.4）。
	ASBindingType e = ASBindingDecl::resolve(make_pi(Variant::INT, StringName(), PROPERTY_HINT_ENUM));
	CHECK(e.valid);
	CHECK(e.as_name == "int64");
}

void as_binding_decl_method_forms() {
	MethodInfo mi;
	mi.name = "get_name";
	mi.return_val = make_pi(Variant::STRING);
	mi.flags = METHOD_FLAG_CONST;
	String decl, reason;
	REQUIRE(ASBindingDecl::method_to_decl(mi, &decl, &reason));
	CHECK(decl == "String get_name() const");

	MethodInfo add;
	add.name = "add_child";
	add.return_val = make_pi(Variant::NIL);
	add.arguments.push_back(make_pi(Variant::OBJECT, "Node"));
	add.flags = METHOD_FLAG_NORMAL;
	CHECK(ASBindingDecl::method_to_decl(add, &decl, &reason));
	CHECK(decl == "void add_child(Node@)");
}

void as_binding_decl_rejects_vararg_and_unknown() {
	MethodInfo vararg;
	vararg.name = "call";
	vararg.return_val = make_pi(Variant::NIL);
	vararg.flags = METHOD_FLAG_VARARG;
	String decl, reason;
	CHECK_FALSE(ASBindingDecl::method_to_decl(vararg, &decl, &reason));
	CHECK(reason.begins_with("vararg"));
}
