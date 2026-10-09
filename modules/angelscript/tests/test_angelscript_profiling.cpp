/**************************************************************************/
/*  test_angelscript_profiling.cpp                                        */
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

#include "../as_engine.h"
#include "../as_script_language.h"

#define ANGELSCRIPT_PROFILING_TESTS_IMPL
#include "test_angelscript_profiling.h"

#ifdef DEBUG_ENABLED

// 采样表按签名归组，导出顺序不保证；统一用签名查找，避免依赖 HashMap 迭代顺序。
static bool _find_profile(const ScriptLanguage::ProfilingInfo *p_arr, int p_count, const StringName &p_signature, ScriptLanguage::ProfilingInfo *r_out) {
	for (int i = 0; i < p_count; i++) {
		if (p_arr[i].signature == p_signature) {
			if (r_out != nullptr) {
				*r_out = p_arr[i];
			}
			return true;
		}
	}
	return false;
}

void as_profiling_counts_and_stops() {
	ASScriptLanguage *lang = ASScriptLanguage::get_singleton();
	REQUIRE(lang != nullptr);
	if (lang == nullptr) {
		return;
	}

	// 默认不采样（Review Focus 1）。
	CHECK(!lang->is_profiling());

	lang->profiling_start();
	CHECK(lang->is_profiling());

	lang->profile_function("as_test::alpha", 10);
	lang->profile_function("as_test::alpha", 25);

	ScriptLanguage::ProfilingInfo info[8];
	const int count = lang->profiling_get_accumulated_data(info, 8);
	ScriptLanguage::ProfilingInfo found;
	const bool has = _find_profile(info, count, "as_test::alpha", &found);
	REQUIRE(has);
	if (!has) {
		return;
	}
	CHECK(found.call_count == 2);
	CHECK(found.total_time == 35);
	CHECK(found.self_time == 35);
	CHECK(found.internal_time == 0);

	// 停止后历史仍可读，且不再累加（Review Focus 3）。
	lang->profiling_stop();
	CHECK(!lang->is_profiling());
	lang->profile_function("as_test::alpha", 100);

	const int count_after = lang->profiling_get_accumulated_data(info, 8);
	ScriptLanguage::ProfilingInfo found_after;
	const bool has_after = _find_profile(info, count_after, "as_test::alpha", &found_after);
	REQUIRE(has_after);
	if (!has_after) {
		return;
	}
	CHECK(found_after.call_count == 2);
	CHECK(found_after.total_time == 35);

	// 重新 start 必须清空历史，避免跨会话脏数据（Review Focus 2）。
	lang->profiling_start();
	const int count_reset = lang->profiling_get_accumulated_data(info, 8);
	CHECK(count_reset == 0);
	lang->profiling_stop();
}

#endif // DEBUG_ENABLED
