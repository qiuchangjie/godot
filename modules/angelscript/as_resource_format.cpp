/**************************************************************************/
/*  as_resource_format.cpp                                                */
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

#include "as_resource_format.h"

#include "as_script.h"

#include "core/io/file_access.h"
#include "core/os/thread.h"

// 加载即编译：把源码交给 ASScript::compile_source()，语法/基类/类名三类错误都在这里拦下，
// 这样编辑器 FileSystem 打开 .as 文件时就能看到明确报错，而不是等到运行时。
Ref<Resource> ASResourceFormatLoaderASScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	// AS 引擎主线程独占（spec §4.4.3）：AngelScript 的模块表与类型注册都没有加锁。
	// ResourceLoader 的同步加载在主线程，但 load_threaded_request() 会在工作线程进入这里，
	// 必须显式拦下，否则会静默破坏引擎状态。
	if (!Thread::is_main_thread()) {
		ERR_PRINT(vformat("AngelScript resources must be loaded on the main thread: '%s'", p_path));
		if (r_error) {
			*r_error = ERR_UNAVAILABLE;
		}
		return Ref<Resource>();
	}

	if (p_path.get_extension().to_lower() == "asb") {
		// `.asb`（预编译字节码）在阶段一只保留扩展名识别；二进制形态的读写与版本校验属于
		// 后续里程碑，这里明确拒绝，避免把二进制当 UTF-8 文本读出一堆无意义诊断。
		ERR_PRINT(vformat("AngelScript bytecode '.asb' is not supported yet: '%s'", p_path));
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		return Ref<Resource>();
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return Ref<Resource>();
	}

	Ref<ASScript> script;
	script.instantiate();
	script->set_source_code(f->get_as_text());
	script->set_path(p_original_path.is_empty() ? p_path : p_original_path);

	String error;
	if (!script->compile_source(script->get_source_code(), script->get_path(), &error)) {
		ERR_PRINT(vformat("Failed to load AngelScript '%s': %s", p_path, error));
		if (r_error) {
			*r_error = ERR_PARSE_ERROR;
		}
		return Ref<Resource>();
	}

	if (r_error) {
		*r_error = OK;
	}
	return script;
}

void ASResourceFormatLoaderASScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("as");
	p_extensions->push_back("asb");
}

bool ASResourceFormatLoaderASScript::handles_type(const String &p_type) const {
	return p_type == "Script" || p_type == "AngelScript";
}

String ASResourceFormatLoaderASScript::get_resource_type(const String &p_path) const {
	const String ext = p_path.get_extension().to_lower();
	return (ext == "as" || ext == "asb") ? "AngelScript" : String();
}

Error ASResourceFormatSaverASScript::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<ASScript> script = p_resource;
	ERR_FAIL_COND_V(script.is_null(), ERR_INVALID_PARAMETER);

	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save AngelScript to '" + p_path + "'.");

	f->store_string(script->get_source_code());
	return OK;
}

void ASResourceFormatSaverASScript::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (p_resource.is_valid() && Object::cast_to<ASScript>(p_resource.ptr())) {
		p_extensions->push_back("as");
	}
}

bool ASResourceFormatSaverASScript::recognize(const Ref<Resource> &p_resource) const {
	return p_resource.is_valid() && Object::cast_to<ASScript>(p_resource.ptr()) != nullptr;
}
