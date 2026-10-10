#pragma once

#include "tests/test_macros.h"

void as_m3_probe_array_and_variant();
void as_m3_variant_object_arg();
void as_m3_property_roundtrip();

#ifndef ANGELSCRIPT_M3_PROPERTY_TESTS_IMPL
TEST_CASE("[AngelScript][M3] probe Array and Variant marshalling") {
	as_m3_probe_array_and_variant();
}

TEST_CASE("[AngelScript][M3] Variant holds object") {
	as_m3_variant_object_arg();
}

TEST_CASE("[AngelScript][M3] property roundtrip") {
	as_m3_property_roundtrip();
}
#endif // ANGELSCRIPT_M3_PROPERTY_TESTS_IMPL
