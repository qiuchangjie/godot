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
#include "as_script_language.h"

#include "core/io/file_access.h"
#include "core/object/script_language.h"
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
		// `.asb` 容器携带基类与所需类型符号表，交由 ASScript 在加载前做惰性注册。
		const String script_path = p_original_path.is_empty() ? p_path : p_original_path;

		Error read_error = OK;
		Vector<uint8_t> bytes = FileAccess::get_file_as_bytes(p_path, &read_error);
		if (read_error != OK) {
			ERR_PRINT(vformat("Failed to read AngelScript bytecode '%s'.", p_path));
			if (r_error) {
				*r_error = read_error;
			}
			return Ref<Resource>();
		}

		// 不在加载器内 set_path：资源缓存（ResourceCache）的登记权归 ResourceLoader——
		// REUSE 模式由它调用 set_path 登记，IGNORE 模式调用 set_path_cache 故意不登记。
		// 加载器若自行 set_path，在编辑器 ScriptEditor::_reload_scripts() 的 CACHE_MODE_IGNORE
		// 重载路径下会撞上缓存中既有的同名内存脚本，报 “possible cyclic resource inclusion”，
		// 随后 path_cache 为空、编译失败并返回 null（表现为 Script Editor 的 rel_scr.is_null()）。
		Ref<ASScript> script;
		script.instantiate();

		String error;
		if (!script->load_bytecode(bytes, script_path, &error)) {
			ERR_PRINT(vformat("Failed to load AngelScript bytecode '%s': %s", p_path, error));
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

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return Ref<Resource>();
	}

	// 路径显式传给 compile_source（类名与模块名都由它推导），但不在加载器内 set_path，
	// 理由同 `.asb` 分支：把缓存登记权留给 ResourceLoader，避免 IGNORE 重载时自撞缓存。
	const String script_path = p_original_path.is_empty() ? p_path : p_original_path;
	Ref<ASScript> script;
	script.instantiate();
	script->set_source_code(f->get_as_text());

	String error;
	if (!script->compile_source(script->get_source_code(), script_path, &error)) {
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

	// 与 GDScript saver 一致：按需在保存后触发重载，编辑器才会刷新 Inspector 的导出属性；
	// 缺这一步，修改 .as 后必须重开工程才能看到新属性。
	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		ASScriptLanguage::get_singleton()->reload_tool_script(script, true);
	}
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
