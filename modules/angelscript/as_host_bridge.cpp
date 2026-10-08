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
