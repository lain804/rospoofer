#pragma once

#define NOMINMAX
#include <Windows.h>
#include <SubAuth.h>

using NtDeleteValueKey_t = NTSTATUS (NTAPI *) (
	IN HANDLE KeyHandle,
	IN PUNICODE_STRING ValueName
);
