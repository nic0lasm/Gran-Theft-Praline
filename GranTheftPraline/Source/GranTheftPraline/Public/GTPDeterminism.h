#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/**
 * -DemoDeterministic=1 makes a run repeatable for A/B measurements: the demo route is
 * driven by simulated game time in fixed steps, every random choice of the traffic and
 * the demo drive comes from its own fixed seed (not from a stream shared with other
 * cars, whose draw order depends on frame timing), and the weather seed is fixed.
 * Without the argument nothing changes.
 */
namespace GTPDeterminism
{
inline bool IsOn()
{
	static const bool bOn = []()
	{
		int32 Value = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("DemoDeterministic="), Value))
		{
			return Value != 0;
		}
		return FParse::Param(FCommandLine::Get(), TEXT("DemoDeterministic"));
	}();
	return bOn;
}
}
