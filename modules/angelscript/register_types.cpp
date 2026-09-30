/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "as_resource_format.h"
#include "as_script_language.h"
#include "binding/as_binding_dumper.h"
#include "binding/as_binding_plan.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"

// 本模块的测试用例写在 tests/*.h 里：modules/SCsub 会把它们汇进生成的
// modules/modules_tests.gen.h，再由 tests/test_main.cpp 统一 include。
// 此处不要再手工 include 测试头，否则用例会被注册两次。

ASScriptLanguage *script_language_as = nullptr;
Ref<ASResourceFormatLoaderASScript> resource_loader_as;
Ref<ASResourceFormatSaverASScript> resource_saver_as;

void initialize_angelscript_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}
	if (script_language_as != nullptr) {
		return; // 幂等：编辑器重启流程可能重复调用。
	}

	script_language_as = memnew(ASScriptLanguage);
	ScriptServer::register_language(script_language_as);

	resource_loader_as.instantiate();
	ResourceLoader::add_resource_format_loader(resource_loader_as);
	resource_saver_as.instantiate();
	ResourceSaver::add_resource_format_saver(resource_saver_as);
}

void uninitialize_angelscript_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}

	if (script_language_as) {
		ScriptServer::unregister_language(script_language_as);
		memdelete(script_language_as);
		script_language_as = nullptr;
	}

	// 部分启动流程（如 Main::test_setup）会在 SERVERS 期先反初始化再重新初始化模块，
	// 第二次反初始化时这里的引用已被清空；加空值守卫，避免无意义的 ERR_PRINT。
	if (resource_loader_as.is_valid()) {
		ResourceLoader::remove_resource_format_loader(resource_loader_as);
		resource_loader_as.unref();
	}
	if (resource_saver_as.is_valid()) {
		ResourceSaver::remove_resource_format_saver(resource_saver_as);
		resource_saver_as.unref();
	}
}

void angelscript_dump_api(const String &p_dir) {
	ASBindingPlan plan;
	plan.build(ASBindingScope::from_project_settings());

	const Error err = ASBindingDumper::write(plan, p_dir);
	if (err != OK) {
		ERR_PRINT(vformat("AngelScript: failed to dump API to '%s' (%d).", p_dir, (int)err));
		return;
	}
	OS::get_singleton()->print("AngelScript API dumped to %s\n", p_dir.utf8().get_data());
}
