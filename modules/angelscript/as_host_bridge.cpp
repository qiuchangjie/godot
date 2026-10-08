/**************************************************************************/
/*  as_host_bridge.cpp                                                    */
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

#include "as_host_bridge.h"

ASHostBridge *ASHostBridge::get_singleton() {
	static ASHostBridge singleton;
	return &singleton;
}

Error ASHostBridge::install(const ASHostCallbacks *p_callbacks) {
	if (p_callbacks == nullptr) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_callbacks->abi_version != AS_HOST_BRIDGE_ABI_VERSION) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_callbacks->struct_size < (uint32_t)sizeof(ASHostCallbacks)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_callbacks->invoke == nullptr) {
		return ERR_INVALID_PARAMETER;
	}
	callbacks = *p_callbacks;
	installed = true;
	return OK;
}

void ASHostBridge::uninstall() {
	callbacks = ASHostCallbacks();
	installed = false;
}

Error ASHostBridge::invoke(int p_method_id, const Variant *p_args, int p_argc, Variant *r_ret) {
	if (!installed || callbacks.invoke == nullptr) {
		return ERR_UNCONFIGURED;
	}
	const int rc = callbacks.invoke(callbacks.user_data, (int32_t)p_method_id, p_args, (int32_t)p_argc, r_ret);
	return rc == 0 ? OK : FAILED;
}
