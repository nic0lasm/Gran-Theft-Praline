#pragma once

#include "CoreMinimal.h"

#include <cstdio>

/**
 * GTP_TRACE(Category, Format, ...): a Log line that survives Shipping.
 * UE_LOG is compiled out of Shipping builds (NO_LOGGING), yet the soak tests
 * (tools/build/test-build-mac.sh) run the Shipping app and read its stdout:
 * there, the line goes to stdout as "Category: message".
 */
#if NO_LOGGING
#define GTP_TRACE(CategoryName, Format, ...) \
	do \
	{ \
		const FString GTPTraceLine = FString::Printf(TEXT("%s: %s"), TEXT(#CategoryName), *FString::Printf(Format, ##__VA_ARGS__)); \
		std::fputs(TCHAR_TO_UTF8(*GTPTraceLine), stdout); \
		std::fputc('\n', stdout); \
		std::fflush(stdout); \
	} while (false)
#else
#define GTP_TRACE(CategoryName, Format, ...) UE_LOG(CategoryName, Log, Format, ##__VA_ARGS__)
#endif
