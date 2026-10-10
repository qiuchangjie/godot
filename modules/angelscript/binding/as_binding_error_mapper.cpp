/**************************************************************************/
/*  as_binding_error_mapper.cpp                                           */
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

#include "as_binding_error_mapper.h"

namespace {

bool _is_ident_start(char32_t p_c) {
	return (p_c >= 'A' && p_c <= 'Z') || (p_c >= 'a' && p_c <= 'z') || p_c == '_';
}

bool _is_ident_char(char32_t p_c) {
	return _is_ident_start(p_c) || (p_c >= '0' && p_c <= '9');
}

} // namespace

bool ASBindingErrorMapper::extract_from_message(const String &p_message, const ASBindingPlan &p_plan, ASBindingMissingSymbols *r_out) {
	ERR_FAIL_NULL_V(r_out, false);

	bool found = false;
	const int len = p_message.length();
	int i = 0;
	while (i < len) {
		if (!_is_ident_start(p_message[i])) {
			i++;
			continue;
		}
		const int start = i;
		while (i < len && _is_ident_char(p_message[i])) {
			i++;
		}
		// 只认可见集合里的类名：消息里的行号、文件名、成员名都不是类名，
		// 误加入会触发一次无效注册。
		const StringName ident = p_message.substr(start, i - start);
		if (p_plan.has_class(ident) && !r_out->types.has(ident)) {
			r_out->types.push_back(ident);
			found = true;
		}
	}
	return found;
}

bool ASBindingErrorMapper::extract(const Vector<String> &p_messages, const ASBindingPlan &p_plan, ASBindingMissingSymbols *r_out) {
	ERR_FAIL_NULL_V(r_out, false);

	bool found = false;
	for (const String &msg : p_messages) {
		if (extract_from_message(msg, p_plan, r_out)) {
			found = true;
		}
	}
	return found;
}
