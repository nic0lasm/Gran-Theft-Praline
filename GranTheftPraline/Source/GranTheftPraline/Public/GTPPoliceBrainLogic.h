#pragma once

// The tactical state of one police unit, written without engine types (tested in
// tools/vehicles/wanted_sim/brain_test.cpp). The rules of the stars stay in
// GTPWantedLogic.h and GTPPoliceTactics.h; this is the graph a StateTree asset
// would encode, kept as plain logic so CI can run it and the StateTree tasks
// only call it. Perception (GTPPoliceSenses) feeds the inputs.
//
//   Idle     no stars
//   Pursue   the target is sensed (by this unit or by radio, anybody's)
//   Search   the target was lost: go to the last known place, then sweep
//   Arrest   the target cannot get away and is within reach
//   Block    a roadblock car: stands across the road until the player is past

namespace GTPPoliceBrain
{
	enum class EState
	{
		Idle,
		Pursue,
		Search,
		Arrest,
		Block
	};

	struct FInput
	{
		int Stars = 0;
		/** Some unit senses the target now (sight or hearing of a loud noise is NOT sight: pass sight only). */
		bool bSensed = false;
		/** Seconds since the target was last sensed. */
		float SecondsSinceSensed = 1000.f;
		float DistanceM = 1000.f;
		/** On foot, or nearly stopped. */
		bool bTargetHelpless = false;
		bool bIsRoadblock = false;
	};

	struct FSettings
	{
		/** Lost for this long before the unit switches from Pursue to Search (s): a corner is not a loss. */
		float LoseGraceSeconds = 1.5f;
		float ArrestEnterM = 7.f;
		/** Leaves Arrest only beyond this (hysteresis, m). */
		float ArrestLeaveM = 11.f;
	};

	inline EState Decide(const FInput& In, EState Previous, const FSettings& S = FSettings())
	{
		if (In.Stars <= 0)
		{
			return EState::Idle;
		}
		if (In.bIsRoadblock)
		{
			return EState::Block;
		}
		if (In.bTargetHelpless)
		{
			const float Limit = Previous == EState::Arrest ? S.ArrestLeaveM : S.ArrestEnterM;
			if (In.DistanceM <= Limit)
			{
				return EState::Arrest;
			}
		}
		if (In.bSensed || In.SecondsSinceSensed <= S.LoseGraceSeconds)
		{
			return EState::Pursue;
		}
		return EState::Search;
	}

	/** Siren and light bar: yelp while pursuing or arresting, two-tone while searching, off when idle. */
	inline bool WantsYelp(EState State)
	{
		return State == EState::Pursue || State == EState::Arrest || State == EState::Block;
	}
}
