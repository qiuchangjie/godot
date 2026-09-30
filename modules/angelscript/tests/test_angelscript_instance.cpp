/**************************************************************************/
/*  test_angelscript_instance.cpp                                         */
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

#include "../as_script.h"

#include "scene/main/node.h"

#define ANGELSCRIPT_INSTANCE_TESTS_IMPL
#include "test_angelscript_instance.h"

static Ref<ASScript> _make_script(const String &p_class, const String &p_body) {
	Ref<ASScript> script;
	script.instantiate();
	String error;
	const String source = "// godot_base: Node\nclass " + p_class + " {\n" + p_body + "\n}\n";
	script->compile_source(source, "res://" + p_class + ".as", &error);
	CHECK_MESSAGE(script->is_valid(), error);
	return script;
}

// 建实例 → 挂到 Node 上 → 返回实例指针；失败返回 nullptr（调用方负责守卫）。
static ScriptInstance *_attach_instance(const Ref<ASScript> &p_script, Node *p_node) {
	ScriptInstance *instance = p_script->instance_create(p_node);
	if (instance == nullptr) {
		return nullptr;
	}
	p_node->set_script_instance(instance);
	return instance;
}

void as_instance_ready_is_dispatched_by_name() {
	Ref<ASScript> script = _make_script("hot_node",
			"\tint ready_count = 0;\n"
			"\tvoid _ready() { ready_count = 7; }\n"
			"\tint get_ready_count() const { return ready_count; }");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}

	Node *node = memnew(Node);
	ScriptInstance *instance = _attach_instance(script, node);
	REQUIRE(instance != nullptr);
	if (instance == nullptr) {
		memdelete(node);
		return;
	}

	CHECK(node->get_script_instance() == instance);
	CHECK(instance->has_method("_ready"));
	CHECK_FALSE(instance->has_method("_not_defined"));
	CHECK(instance->call("_ready") == Variant());

	Variant call_result;
	Callable::CallError error;
	call_result = instance->callp("get_ready_count", nullptr, 0, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	CHECK(call_result == Variant(7));

	Variant property_value;
	CHECK(instance->get("ready_count", property_value));
	CHECK(property_value == Variant(7));

	memdelete(node);
}

void as_instance_notification_reaches_notification() {
	Ref<ASScript> script = _make_script("notified_node",
			"\tint last_notification = 0;\n"
			"\tvoid _notification(int what) { last_notification = what; }\n"
			"\tint get_last_notification() const { return last_notification; }");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}

	Node *node = memnew(Node);
	ScriptInstance *instance = _attach_instance(script, node);
	REQUIRE(instance != nullptr);
	if (instance == nullptr) {
		memdelete(node);
		return;
	}

	CHECK(instance->has_method("_notification"));
	instance->notification(42);

	Variant call_result;
	Callable::CallError error;
	call_result = instance->callp("get_last_notification", nullptr, 0, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	CHECK(call_result == Variant(42));

	memdelete(node);
}

void as_instance_process_receives_delta() {
	Ref<ASScript> script = _make_script("processing_node",
			"\tdouble last_delta = 0.0;\n"
			"\tvoid _process(double delta) { last_delta = delta; }\n"
			"\tdouble get_last_delta() const { return last_delta; }");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}

	Node *node = memnew(Node);
	ScriptInstance *instance = _attach_instance(script, node);
	REQUIRE(instance != nullptr);
	if (instance == nullptr) {
		memdelete(node);
		return;
	}

	CHECK(instance->has_method("_process"));
	CHECK(instance->call("_process", 0.25) == Variant());

	Variant call_result;
	Callable::CallError error;
	call_result = instance->callp("get_last_delta", nullptr, 0, error);
	CHECK(error.error == Callable::CallError::CALL_OK);
	CHECK(call_result == Variant(0.25));

	memdelete(node);
}

void as_instance_signature_mismatch_is_not_exposed() {
	Ref<ASScript> script = _make_script("mismatched_node",
			"\tvoid _process() { }");
	REQUIRE(script.is_valid());
	if (!script.is_valid()) {
		return;
	}

	Node *node = memnew(Node);
	ScriptInstance *instance = _attach_instance(script, node);
	REQUIRE(instance != nullptr);
	if (instance == nullptr) {
		memdelete(node);
		return;
	}

	// _process 是引擎虚拟方法，签名不匹配（0 参而非 1 参）就当它不存在。
	CHECK_FALSE(instance->has_method("_process"));

	memdelete(node);
}
