/**************************************************************************/
/*  as_binding_scanner.cpp                                                */
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

#include "as_binding_scanner.h"

#include "core/object/class_db.h"

bool ASBindingScanner::_is_ident_start(char32_t p_c) {
	return (p_c >= 'A' && p_c <= 'Z') || (p_c >= 'a' && p_c <= 'z') || p_c == '_';
}

bool ASBindingScanner::_is_ident_char(char32_t p_c) {
	return _is_ident_start(p_c) || (p_c >= '0' && p_c <= '9');
}

void ASBindingScanner::scan_source(const String &p_source, HashSet<StringName> &r_types) {
	const int len = p_source.length();
	int i = 0;
	while (i < len) {
		const char32_t c = p_source[i];

		// 跳过行注释。
		if (c == '/' && i + 1 < len && p_source[i + 1] == '/') {
			while (i < len && p_source[i] != '\n') {
				i++;
			}
			continue;
		}
		// 跳过块注释。
		if (c == '/' && i + 1 < len && p_source[i + 1] == '*') {
			i += 2;
			while (i + 1 < len && !(p_source[i] == '*' && p_source[i + 1] == '/')) {
				i++;
			}
			i = (i + 1 < len) ? i + 2 : len;
			continue;
		}
		// 跳过字符串/字符字面量。
		if (c == '"' || c == '\'') {
			const char32_t quote = c;
			i++;
			while (i < len && p_source[i] != quote) {
				if (p_source[i] == '\\' && i + 1 < len) {
					i++;
				}
				i++;
			}
			i++;
			continue;
		}

		if (!_is_ident_start(c)) {
			i++;
			continue;
		}

		const int start = i;
		while (i < len && _is_ident_char(p_source[i])) {
			i++;
		}
		const StringName ident = p_source.substr(start, i - start);
		if (ClassDB::class_exists(ident)) {
			r_types.insert(ident);
		}
	}
}

Vector<StringName> ASBindingScanner::scan_project(const Vector<String> &p_sources) {
	HashSet<StringName> seen;
	Vector<StringName> ordered;
	for (const String &src : p_sources) {
		HashSet<StringName> found;
		scan_source(src, found);
		for (const StringName &name : found) {
			if (seen.has(name)) {
				continue;
			}
			seen.insert(name);
			ordered.push_back(name);
		}
	}
	return ordered;
}
