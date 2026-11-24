// Checks the hit, fall and ragdoll rules (GTPImpactLogic.h) without Unreal.
// Build and run: python3 tools/ragdoll/check_impact_logic.py
#include <cmath>
#include <cstdio>
#include <string>

#include "../../GranTheftPraline/Source/GranTheftPraline/Public/GTPBreakableLogic.h"
#include "../../GranTheftPraline/Source/GranTheftPraline/Public/GTPHitLogic.h"
#include "../../GranTheftPraline/Source/GranTheftPraline/Public/GTPImpactLogic.h"
#include "../../GranTheftPraline/Source/GranTheftPraline/Public/GTPThreatLogic.h"

using namespace GTPImpact;

static int Failures = 0;
#define CHECK(Cond, ...)                         \
	do                                           \
	{                                            \
		if (!(Cond))                             \
		{                                        \
			++Failures;                          \
			std::printf("FAIL %s: ", #Cond);     \
			std::printf(__VA_ARGS__);            \
			std::printf("\n");                   \
		}                                        \
	} while (0)

static FCarHitResult Hit(float Kmh, float PersonKmhToward = 0.f)
{
	FCarHitInput In;
	In.CarVelocity = {CmSFromKmh(Kmh), 0.f, 0.f}; // car drives east
	In.PersonVelocity = {-CmSFromKmh(PersonKmhToward), 0.f, 0.f};
	In.ToCar = {-100.f, 0.f, 0.f}; // car is west of the person
	return ClassifyCarHit(In, FSettings());
}

int main()
{
	const FSettings S;

	// 1. Severity and damage never decrease with speed.
	ESeverity Prev = ESeverity::None;
	float PrevDamage = 0.f;
	for (float K = 0.f; K <= 120.f; K += 0.5f)
	{
		const FCarHitResult R = Hit(K);
		CHECK(R.Severity >= Prev, "severity decreased at %.1f km/h", K);
		CHECK(R.Damage + 1e-3f >= PrevDamage, "damage decreased at %.1f km/h", K);
		CHECK(R.Damage >= 0.f && R.Damage <= 100.f, "damage out of range at %.1f km/h", K);
		CHECK(std::isfinite(R.LaunchVelocity.X) && std::isfinite(R.LaunchVelocity.Z), "nan at %.1f", K);
		Prev = R.Severity;
		PrevDamage = R.Damage;
	}

	// 2. Reference points: crawl = nothing, 10 = bump, 20 = knock, 40 = launch, 70 = heavy and fatal.
	CHECK(Hit(2.f).Severity == ESeverity::None, "2 km/h");
	CHECK(Hit(8.f).Severity == ESeverity::Bump, "8 km/h");
	CHECK(Hit(20.f).Severity == ESeverity::Knock, "20 km/h");
	CHECK(Hit(40.f).Severity == ESeverity::Launch, "40 km/h");
	CHECK(Hit(70.f).Severity == ESeverity::Heavy, "70 km/h");
	CHECK(Hit(70.f).Damage >= 100.f - 1e-3f, "70 km/h must kill");
	CHECK(Hit(20.f).Damage < 40.f, "20 km/h must not kill");
	CHECK(Hit(40.f).Damage > 40.f && Hit(40.f).Damage < 100.f, "40 km/h hurts, does not kill");

	// 3. Launch goes the way the car drives, upward for launches, bounded.
	const FCarHitResult L = Hit(50.f);
	CHECK(L.LaunchVelocity.X > 0.f && std::fabs(L.LaunchVelocity.Y) < 1e-3f, "launch direction");
	CHECK(L.LaunchVelocity.Z > 0.f, "launch lift");
	CHECK(Length(L.LaunchVelocity) <= S.MaxLaunchCmS * 1.5f, "launch bound");
	CHECK(L.LaunchVelocity.X < CmSFromKmh(50.f), "the body leaves slower than the car");
	CHECK(L.CarSpeedLossKmh > 0.f && L.CarSpeedLossKmh < 5.f, "car loses a little: %.2f", L.CarSpeedLossKmh);

	// 4. A car backing away, or a person running away as fast, hurts no one.
	FCarHitInput Away;
	Away.CarVelocity = {CmSFromKmh(-30.f), 0.f, 0.f};
	Away.ToCar = {-100.f, 0.f, 0.f};
	CHECK(ClassifyCarHit(Away, S).Severity == ESeverity::None, "car moving away");
	CHECK(Hit(30.f, 30.f).RelativeKmh > Hit(30.f, 0.f).RelativeKmh, "a person walking into the car makes it worse");
	FCarHitInput Still;
	CHECK(ClassifyCarHit(Still, S).Severity == ESeverity::None, "nothing moves");

	// 5. A glancing hit is milder than a head-on one.
	FCarHitInput Glance;
	Glance.CarVelocity = {CmSFromKmh(40.f), 0.f, 0.f};
	Glance.ToCar = {-10.f, -100.f, 0.f}; // contact point mostly to the side
	CHECK(ClassifyCarHit(Glance, S).RelativeKmh <= Hit(40.f).RelativeKmh, "glancing is milder");

	// 6. Landings.
	CHECK(ClassifyLanding(400.f, false, S).Kind == ELanding::None, "small drop");
	CHECK(ClassifyLanding(750.f, false, S).Kind == ELanding::Stumble, "stumble");
	CHECK(ClassifyLanding(1000.f, false, S).Kind == ELanding::Roll, "roll");
	CHECK(ClassifyLanding(1500.f, false, S).Kind == ELanding::Ragdoll, "ragdoll");
	CHECK(ClassifyLanding(1000.f, true, S).Kind < ClassifyLanding(1000.f, false, S).Kind, "crouch helps");
	CHECK(ClassifyLanding(1500.f, false, S).Damage > ClassifyLanding(750.f, false, S).Damage, "more speed, more damage");

	// 7. Bailing out of a car.
	CHECK(ClassifyBailOut(5.f) == ESeverity::None, "bail 5");
	CHECK(ClassifyBailOut(35.f) == ESeverity::Knock, "bail 35");
	CHECK(ClassifyBailOut(70.f) == ESeverity::Launch, "bail 70");

	// 8. Ragdoll lifecycle: a knocked person falls, settles, pauses and gets up; a heavy hit stays down.
	const FRagdollTimings T;
	{
		FRagdollState St = StartRagdollState(ESeverity::Knock, 80.f);
		EOutcome O = EOutcome::Simulating;
		float Time = 0.f;
		while (O == EOutcome::Simulating && Time < 30.f)
		{
			const float Speed = Time < 1.f ? 400.f : 5.f; // moving for 1 s, then still
			O = StepRagdoll(St, Speed, 0.7f, 0.05f, T);
			Time += 0.05f;
		}
		CHECK(O == EOutcome::GetUpFaceUp, "knock gets up face up");
		CHECK(Time >= T.MinSeconds, "never earlier than the minimum: %.2f", Time);
		CHECK(Time < T.MinSeconds + T.RestSeconds + T.PauseKnock + 1.f, "gets up soon enough: %.2f", Time);
	}
	{
		FRagdollState St = StartRagdollState(ESeverity::Launch, 40.f);
		EOutcome O = EOutcome::Simulating;
		for (float Time = 0.f; Time < 30.f && O == EOutcome::Simulating; Time += 0.05f)
		{
			O = StepRagdoll(St, 3.f, -0.4f, 0.05f, T);
		}
		CHECK(O == EOutcome::GetUpFaceDown, "face down get-up");
	}
	{
		FRagdollState St = StartRagdollState(ESeverity::Heavy, 0.f);
		EOutcome O = EOutcome::Simulating;
		for (float Time = 0.f; Time < 30.f && O == EOutcome::Simulating; Time += 0.05f)
		{
			O = StepRagdoll(St, 3.f, 0.5f, 0.05f, T);
		}
		CHECK(O == EOutcome::StayDown, "heavy hit stays down");
	}
	{
		// A body that never stops sliding is decided at MaxSeconds (+ pause).
		FRagdollState St = StartRagdollState(ESeverity::Knock, 90.f);
		EOutcome O = EOutcome::Simulating;
		float Time = 0.f;
		for (; Time < 40.f && O == EOutcome::Simulating; Time += 0.05f)
		{
			O = StepRagdoll(St, 500.f, 0.5f, 0.05f, T);
		}
		CHECK(O != EOutcome::Simulating && Time <= T.MaxSeconds + T.PauseKnock + 0.2f, "sliding body decided in time: %.2f", Time);
	}

	// 9. Ragdoll cap freezes the farthest.
	{
		std::vector<FRagdollEntry> A = {{500.f, 3.f}, {4000.f, 1.f}, {900.f, 8.f}, {1500.f, 2.f}, {200.f, 0.5f}};
		const std::vector<int> F = ChooseToFreeze(A, 3);
		CHECK(F.size() == 2, "freeze count %zu", F.size());
		bool Has1 = false, Has3 = false;
		for (int I : F) { Has1 |= (I == 1); Has3 |= (I == 3); }
		CHECK(Has1 && Has3, "freezes the two farthest");
		CHECK(ChooseToFreeze(A, 10).empty(), "under the cap, nothing frozen");
		CHECK(ChooseToFreeze(A, 0).size() == A.size(), "cap 0 freezes all");
	}

	// 8b. Lite timings settle sooner than the normal ones.
	{
		auto Settle = [](const FRagdollTimings& TT) {
			FRagdollState St = StartRagdollState(ESeverity::Launch, 50.f);
			float Time = 0.f;
			EOutcome O = EOutcome::Simulating;
			for (; Time < 30.f && O == EOutcome::Simulating; Time += 0.05f)
			{
				O = StepRagdoll(St, 3.f, 0.5f, 0.05f, TT);
			}
			return Time;
		};
		const float Normal = Settle(FRagdollTimings());
		const float Lite = Settle(LiteTimings());
		CHECK(Lite < Normal * 0.7f, "lite %.2f vs normal %.2f", Lite, Normal);
	}

	// 10. Street furniture.
	{
		namespace B = GTPBreak;
		const float Car = 1200.f;
		const float Walk = CmSFromKmh(5.f);
		const float Slow = CmSFromKmh(2.f);
		CHECK(B::Resolve(B::EKind::Tippable, 15.f, 80.f, Slow).Outcome == B::EOutcome::None, "bin ignores a 2 km/h touch");
		CHECK(B::Resolve(B::EKind::Tippable, 15.f, 80.f, Walk).Outcome == B::EOutcome::Topple, "a jogger tips a bin");
		CHECK(B::Resolve(B::EKind::Tippable, 110.f, Car, CmSFromKmh(20.f)).Outcome == B::EOutcome::Topple, "a car tips a scooter");
		CHECK(B::Resolve(B::EKind::Fixed, 500.f, Car, CmSFromKmh(30.f)).Outcome == B::EOutcome::Stop, "bollards stop the car");
		CHECK(B::Resolve(B::EKind::Fixed, 500.f, 80.f, Slow).Outcome == B::EOutcome::None, "no stop at 2 km/h");
		// A 250 kg street lamp: a walking person does not break it, a car at 20 km/h does.
		CHECK(B::Resolve(B::EKind::Breakable, 250.f, 80.f, Walk).Outcome == B::EOutcome::None, "person cannot break a lamp");
		CHECK(B::Resolve(B::EKind::Breakable, 250.f, Car, CmSFromKmh(5.f)).Outcome == B::EOutcome::None, "5 km/h does not break a lamp");
		CHECK(B::Resolve(B::EKind::Breakable, 250.f, Car, CmSFromKmh(25.f)).Outcome == B::EOutcome::None, "25 km/h does not break an anchored lamp");
		CHECK(B::Resolve(B::EKind::Breakable, 250.f, Car, CmSFromKmh(38.f)).Outcome == B::EOutcome::Break, "a car at 38 km/h breaks a lamp");
		// A 25 kg sign breaks at a low car speed; a light breakable only tips when walked into.
		CHECK(B::Resolve(B::EKind::Breakable, 25.f, Car, CmSFromKmh(8.f)).Outcome == B::EOutcome::Break, "sign breaks at 8 km/h");
		CHECK(B::Resolve(B::EKind::Breakable, 25.f, 80.f, Walk).Outcome == B::EOutcome::Topple, "a person tips a sign");
		// The harder the hit, the longer the throw, bounded; the car slows a little, never stops.
		float Prev = 0.f;
		for (float K = 5.f; K <= 80.f; K += 5.f)
		{
			const B::FResult R = B::Resolve(B::EKind::Breakable, 80.f, Car, CmSFromKmh(K));
			CHECK(R.ThrowCmS + 1e-3f >= Prev, "throw decreased at %.0f", K);
			CHECK(R.ThrowCmS <= B::FSettings().MaxThrowCmS + 1e-3f, "throw bound");
			CHECK(R.ImpactorLossKmh < K * 0.3f, "car loses too much at %.0f km/h: %.2f", K, R.ImpactorLossKmh);
			Prev = R.ThrowCmS;
		}
	}

	// 11. Localised hit reactions.
	{
		namespace H = GTPHit;
		CHECK(H::ZoneFromBone("head") == H::EZone::Head && H::ZoneFromBone("neck_01") == H::EZone::Head, "head zone");
		CHECK(H::ZoneFromBone("lowerarm_l") == H::EZone::Arm && H::ZoneFromBone("hand_r") == H::EZone::Arm, "arm zone");
		CHECK(H::ZoneFromBone("calf_r") == H::EZone::Leg && H::ZoneFromBone("foot_l") == H::EZone::Leg, "leg zone");
		CHECK(H::ZoneFromBone("spine_03") == H::EZone::Torso && H::ZoneFromBone("pelvis") == H::EZone::Torso, "torso zone");

		// A body facing +x (yaw 0): a hit travelling toward -x comes from the front, toward +x from behind.
		CHECK(H::DirectionOf(-1.f, 0.f, 0.f) == H::EDirection::Front, "front");
		CHECK(H::DirectionOf(1.f, 0.f, 0.f) == H::EDirection::Back, "back");
		CHECK(H::DirectionOf(0.f, -1.f, 0.f) == H::EDirection::Left, "from the left (+y is left of +x)");
		CHECK(H::DirectionOf(0.f, 1.f, 0.f) == H::EDirection::Right, "from the right");
		CHECK(H::DirectionOf(0.f, 1.f, 90.f) == H::EDirection::Back, "facing +y, hit travelling +y comes from behind");
		CHECK(H::DirectionOf(1.f, 0.f, 180.f) == H::EDirection::Front, "facing -x, hit travelling +x comes from the front");

		auto Hit = [](float Damage, float HealthAfter, H::EZone Zone, bool Melee = false) {
			H::FHitInfo I;
			I.Damage = Damage;
			I.HealthAfter = HealthAfter;
			I.Zone = Zone;
			I.bMelee = Melee;
			I.DirX = -1.f;
			return I;
		};
		H::FState St;
		CHECK(H::Evaluate(Hit(3.f, 97.f, H::EZone::Torso), St, 0.0).Kind == H::EReaction::None, "tiny hit: nothing");
		H::FState St2;
		CHECK(H::Evaluate(Hit(10.f, 90.f, H::EZone::Torso), St2, 0.0).Kind == H::EReaction::Flinch, "10 damage flinches");
		H::FState St3;
		CHECK(H::Evaluate(Hit(25.f, 75.f, H::EZone::Torso), St3, 0.0).Kind == H::EReaction::Stagger, "25 damage staggers");
		H::FState St4;
		CHECK(H::Evaluate(Hit(50.f, 50.f, H::EZone::Torso), St4, 0.0).Kind == H::EReaction::Knockdown, "50 damage knocks down");
		H::FState St5;
		const H::FReaction Kd = H::Evaluate(Hit(50.f, 50.f, H::EZone::Torso), St5, 0.0);
		CHECK(Kd.LaunchX < 0.f && Kd.LaunchZ > 0.f && std::fabs(Kd.LaunchX) <= 650.f, "launch along the hit, bounded");
		H::FState St6;
		const H::EReaction HeadKind = H::Evaluate(Hit(8.f, 92.f, H::EZone::Head), St6, 0.0).Kind;
		H::FState St6b;
		const H::EReaction LegKind = H::Evaluate(Hit(8.f, 92.f, H::EZone::Leg), St6b, 0.0).Kind;
		CHECK(HeadKind > LegKind, "a head hit reacts more than a leg hit");
		H::FState St7;
		CHECK(H::Evaluate(Hit(10.f, 0.f, H::EZone::Torso), St7, 0.0).Kind == H::EReaction::Death, "health 0 dies");
		CHECK(H::Evaluate(Hit(50.f, 0.f, H::EZone::Torso), St7, 1.0).Kind == H::EReaction::None, "the dead do not react again");
		// Cooldown, chain and immunity.
		H::FState C;
		CHECK(H::Evaluate(Hit(10.f, 90.f, H::EZone::Torso), C, 0.0).Kind == H::EReaction::Flinch, "first flinch");
		CHECK(H::Evaluate(Hit(10.f, 90.f, H::EZone::Torso), C, 0.1).Kind == H::EReaction::None, "cooldown swallows the next");
		CHECK(H::Evaluate(Hit(10.f, 90.f, H::EZone::Torso), C, 0.5).Kind == H::EReaction::Flinch, "second after the cooldown");
		CHECK(H::Evaluate(Hit(10.f, 90.f, H::EZone::Torso), C, 1.0).Kind == H::EReaction::Stagger, "the third flinch in 2 s staggers");
		H::FState K;
		CHECK(H::Evaluate(Hit(60.f, 40.f, H::EZone::Torso), K, 0.0).Kind == H::EReaction::Knockdown, "knockdown");
		CHECK(H::Evaluate(Hit(60.f, 40.f, H::EZone::Torso), K, 1.0).Kind == H::EReaction::Stagger, "immune to a second knockdown for 2 s");
		CHECK(H::Evaluate(Hit(60.f, 40.f, H::EZone::Torso), K, 3.0).Kind == H::EReaction::Knockdown, "knockdown again after the immunity");
		H::FState Hv;
		H::FHitInfo Heavy = Hit(18.f, 82.f, H::EZone::Torso, true);
		CHECK(H::Evaluate(Heavy, Hv, 0.0).Kind != H::EReaction::Knockdown, "18 melee damage alone does not knock down");
		H::FState Hv2;
		Heavy.bHeavy = true;
		CHECK(H::Evaluate(Heavy, Hv2, 0.0).Kind == H::EReaction::Knockdown, "a heavy blow knocks down at 18 damage");
		H::FState Hv3;
		CHECK(H::Evaluate(Hit(18.f, 0.f, H::EZone::Torso, true), Hv3, 0.0).Kind == H::EReaction::Death, "death still wins over heavy");
		CHECK(std::string(H::ClipName(H::EReaction::Flinch, H::EDirection::Back)) == "AM_HitLight_Back", "clip name");
		CHECK(std::string(H::ClipName(H::EReaction::Stagger, H::EDirection::Front)) == "AM_Stagger_Front", "stagger clip name");
	}

	// 12. Threat response.
	{
		namespace T = GTPThreat;
		auto Run = [](T::FBrain& B, const T::FPerception& P, float Seconds, T::EState* StopAt = nullptr) {
			float Time = 0.f;
			for (; Time < Seconds; Time += 0.1f)
			{
				T::Step(B, P, 0.1f);
				if (StopAt && B.State == *StopAt) break;
			}
			return Time;
		};
		T::FPerception Aimed;
		Aimed.bAimedAt = true;
		Aimed.DistanceToThreatCm = 400.f;

		// Compliant: alarmed, then hands up, then obeying, within a few seconds.
		{
			T::FBrain B;
			T::EState Target = T::EState::Complying;
			const float Time = Run(B, Aimed, 10.f, &Target);
			CHECK(B.State == T::EState::Complying, "a compliant person ends up complying (state %d)", static_cast<int>(B.State));
			CHECK(Time > 0.9f && Time < 5.0f, "complying after %.1f s", Time);
			CHECK(T::ObeysOrders(B), "obeys orders");
			CHECK(B.SinceAimed < 0.2f, "aimed counter");
			// The threat leaves: hands up, then alarmed, then calm; the fear decays.
			T::FPerception None;
			Run(B, None, 12.f);
			CHECK(B.State == T::EState::Alarmed || B.State == T::EState::Calm, "relaxes (state %d)", static_cast<int>(B.State));
			Run(B, None, 30.f);
			CHECK(B.State == T::EState::Calm && B.Fear < 0.1f, "calm again, fear %.2f", B.Fear);
		}
		// Unarmed threat: nothing happens.
		{
			T::FBrain B;
			T::FPerception Hand = Aimed;
			Hand.bThreatArmed = false;
			Run(B, Hand, 20.f);
			CHECK(B.State == T::EState::Calm && B.Fear == 0.f, "an empty hand frightens no one");
		}
		// The hold-up rule. Within the hold-up range everybody but the brave surrenders at once and stays so while held.
		for (const T::ETemperament Who : {T::ETemperament::Compliant, T::ETemperament::Frozen, T::ETemperament::Fleeing})
		{
			T::FBrain B;
			B.Temperament = Who;
			T::Step(B, Aimed, 0.1f);
			CHECK(B.State == T::EState::HandsUp, "temperament %d surrenders at once at 4 m (state %d)", static_cast<int>(Who), static_cast<int>(B.State));
			Run(B, Aimed, 30.f);
			CHECK(B.State == T::EState::Complying, "temperament %d stays held for 30 s (state %d)", static_cast<int>(Who), static_cast<int>(B.State));
			// The gunman steps back to 9.5 m, still aiming: still held (inside the keep range).
			T::FPerception Back = Aimed;
			Back.DistanceToThreatCm = 950.f;
			Run(B, Back, 10.f);
			CHECK(B.State == T::EState::Complying, "temperament %d still held from 9.5 m (state %d)", static_cast<int>(Who), static_cast<int>(B.State));
		}
		// Beyond the hold-up range: the compliant and the fearful run, the frozen freezes with his hands up.
		{
			T::FPerception Far = Aimed;
			Far.DistanceToThreatCm = 1500.f;
			for (const T::ETemperament Who : {T::ETemperament::Compliant, T::ETemperament::Fleeing})
			{
				T::FBrain B;
				B.Temperament = Who;
				T::EState Target = T::EState::Fleeing;
				const float Time = Run(B, Far, 10.f, &Target);
				CHECK(B.State == T::EState::Fleeing, "temperament %d aimed at from 15 m runs (state %d)", static_cast<int>(Who), static_cast<int>(B.State));
				CHECK(Time < 4.f, "and soon: %.1f s", Time);
				// Who runs does not stop for a gun, even a close one.
				Run(B, Aimed, 3.f);
				CHECK(B.State == T::EState::Fleeing, "temperament %d keeps running under a close gun (state %d)", static_cast<int>(Who), static_cast<int>(B.State));
			}
			T::FBrain F;
			F.Temperament = T::ETemperament::Frozen;
			Run(F, Far, 10.f);
			CHECK(F.State == T::EState::HandsUp || F.State == T::EState::Complying, "a frozen person aimed at from 15 m freezes (state %d)", static_cast<int>(F.State));
		}
		// Just inside and just outside the range.
		{
			T::FPerception In = Aimed, Out = Aimed;
			In.DistanceToThreatCm = 790.f;
			Out.DistanceToThreatCm = 810.f;
			T::FBrain A, B;
			Run(A, In, 5.f);
			Run(B, Out, 5.f);
			CHECK(A.State == T::EState::Complying, "7.9 m: held (state %d)", static_cast<int>(A.State));
			CHECK(B.State == T::EState::Fleeing, "8.1 m: runs (state %d)", static_cast<int>(B.State));
		}
		// The gun looks away: the fearful bolts after a second, the compliant lowers his hands after a few and does not run.
		{
			T::FPerception None;
			T::FBrain B;
			B.Temperament = T::ETemperament::Fleeing;
			Run(B, Aimed, 5.f);
			CHECK(B.State == T::EState::Complying, "fearful held (state %d)", static_cast<int>(B.State));
			Run(B, None, 0.8f);
			CHECK(B.State != T::EState::Fleeing, "not before a second (state %d)", static_cast<int>(B.State));
			Run(B, None, 0.6f);
			CHECK(B.State == T::EState::Fleeing, "the fearful bolts when the gun looks away (state %d)", static_cast<int>(B.State));
			T::FBrain C;
			Run(C, Aimed, 5.f);
			bool bRan = false;
			for (float Time = 0.f; Time < 30.f; Time += 0.1f)
			{
				T::Step(C, None, 0.1f);
				bRan |= C.State == T::EState::Fleeing;
			}
			CHECK(!bRan && C.State == T::EState::Calm, "the compliant lowers his hands and calms down (state %d)", static_cast<int>(C.State));
		}
		// A shot while held: the fearful bolts at once, the frozen never.
		{
			T::FBrain B;
			B.Temperament = T::ETemperament::Fleeing;
			Run(B, Aimed, 3.f);
			T::FPerception Fire = Aimed;
			Fire.bShotNearby = true;
			T::Step(B, Fire, 0.1f);
			CHECK(B.State == T::EState::Fleeing, "a shot frees the fearful (state %d)", static_cast<int>(B.State));
		}
		// Frozen never runs, even under fire.
		{
			T::FBrain F;
			F.Temperament = T::ETemperament::Frozen;
			T::FPerception Fire = Aimed;
			Fire.bShotAtMe = true;
			Run(F, Fire, 20.f);
			CHECK(F.State == T::EState::Complying || F.State == T::EState::HandsUp, "frozen stays with the hands up (state %d)", static_cast<int>(F.State));
		}
		// Brave: backs away at close range before putting the hands up.
		{
			T::FBrain B;
			B.Temperament = T::ETemperament::Brave;
			T::FPerception Close = Aimed;
			Close.DistanceToThreatCm = 250.f;
			bool bBacked = false;
			for (float Time = 0.f; Time < 15.f; Time += 0.1f)
			{
				T::Step(B, Close, 0.1f);
				bBacked |= B.State == T::EState::Backing;
			}
			CHECK(bBacked, "a brave person backs away first");
			CHECK(B.State == T::EState::HandsUp || B.State == T::EState::Complying || B.State == T::EState::Fleeing, "then gives in (state %d)", static_cast<int>(B.State));
		}
		// Contagion: a calm person next to a terrified one becomes alarmed; shots frighten people nearby.
		{
			T::FBrain B;
			T::FPerception Near;
			Near.NearbyFear = 0.9f;
			Run(B, Near, 6.f);
			CHECK(B.State != T::EState::Calm, "fear spreads (state %d)", static_cast<int>(B.State));
			T::FBrain C;
			T::FPerception Shot;
			Shot.bShotNearby = true;
			T::Step(C, Shot, 0.1f);
			CHECK(C.Fear > 0.3f, "a shot nearby frightens: %.2f", C.Fear);
		}
		// Hits: a knockdown takes the body down, then he gets up; a death stays.
		{
			T::FBrain B;
			T::OnHit(B, true, false);
			CHECK(B.State == T::EState::Down, "knockdown goes down");
			T::FPerception None;
			Run(B, None, 4.f);
			CHECK(B.State == T::EState::Fleeing, "gets up and runs (state %d)", static_cast<int>(B.State));
			T::FBrain D;
			T::OnHit(D, false, true);
			Run(D, Aimed, 20.f);
			CHECK(D.State == T::EState::Down && D.bDead, "the dead stay down");
			T::FBrain Fl;
			T::OnHit(Fl, false, false);
			CHECK(Fl.Fear > 0.2f, "a flinch frightens");
		}
		// Fear stays in 0..1 whatever happens.
		{
			T::FBrain B;
			T::FPerception Everything = Aimed;
			Everything.bShotAtMe = Everything.bShotNearby = true;
			Everything.NearbyFear = 1.f;
			for (int I = 0; I < 300; ++I)
			{
				T::Step(B, Everything, 0.1f);
				CHECK(B.Fear >= 0.f && B.Fear <= 1.f, "fear out of range %.2f", B.Fear);
			}
		}
		// Aim cone.
		CHECK(T::InAimCone(0, 0, 160, 1, 0, 0, 500, 20, 160, 2000, 10), "slightly off the line is still aimed at");
		CHECK(!T::InAimCone(0, 0, 160, 1, 0, 0, 500, 200, 160, 2000, 10), "outside the cone");
		CHECK(!T::InAimCone(0, 0, 160, 1, 0, 0, 3000, 0, 160, 2000, 10), "out of range");
		CHECK(!T::InAimCone(0, 0, 160, 1, 0, 0, -500, 0, 160, 2000, 10), "behind the aim");
	}

	// A body thrown by a car stays within bounds, whatever the physics does to it.
	{
		const FBodyBounds B;
		const float Car40 = CmSFromKmh(40.f);
		// A part kicked at 20 m/s upward and 25 m/s ahead by the car's collision.
		const FVec Kicked = BoundVelocity({2500.f, 0.f, 2000.f}, 95.f, Car40, B);
		CHECK(Kicked.Z <= B.MaxUpCmS + 1e-3f, "upward speed bounded: %.0f", Kicked.Z);
		CHECK(Length2D(Kicked) <= Car40 * B.CarShare + 1e-2f, "no faster than the car: %.0f", Length2D(Kicked));
		CHECK(BoundVelocity({100.f, 50.f, -900.f}, 95.f, Car40, B).Z == -900.f, "a fall is left alone");
		CHECK(BoundVelocity({100.f, 50.f, 200.f}, B.MaxHeightCm + 1.f, Car40, B).Z == 0.f, "nothing goes up above the height bound");
		CHECK(HorizontalCap(0.f, B) == B.MinHorizontalCmS, "a floor for the cap");
		const float Nan = std::nanf("");
		CHECK(Length(BoundVelocity({Nan, 0.f, 0.f}, 95.f, Car40, B)) == 0.f, "a bad number becomes rest");
		CHECK(Length(BoundSpin({40.f, 30.f, 0.f}, B)) <= B.MaxSpinRadS + 1e-3f, "spin bounded");
		CHECK(Length(BoundSpin({1.f, 2.f, 0.f}, B)) < 3.f, "a slow spin is left alone");

		// The worst the physics can do: every frame it kicks the pelvis upward at 20 m/s (a car's bonnet under it).
		// With the bounds the pelvis never passes the height bound by more than one frame's travel; without, it flies.
		for (const float Dt : {1.f / 30.f, 1.f / 60.f, 1.f / 120.f})
		{
			float Height = 95.f, Highest = 0.f, Vz = 0.f;
			for (float T = 0.f; T < 3.f; T += Dt)
			{
				Vz = 2000.f; // the kick
				Vz = BoundVelocity({0.f, 0.f, Vz}, Height, Car40, B).Z;
				Height = std::max(15.f, Height + Vz * Dt);
				Vz -= 980.f * Dt;
				Highest = std::max(Highest, Height);
			}
			CHECK(Highest <= B.MaxHeightCm + B.MaxUpCmS * Dt + 1e-2f, "kicked every frame at dt %.4f: highest %.0f cm", Dt, Highest);
		}
		// One kick, then free flight: the top of the throw.
		{
			float Height = 95.f, Highest = 0.f;
			float Vz = BoundVelocity({0.f, 0.f, 2000.f}, Height, Car40, B).Z;
			for (float T = 0.f; T < 3.f; T += 1.f / 60.f)
			{
				Height = std::max(15.f, Height + Vz / 60.f);
				Vz -= 980.f / 60.f;
				Highest = std::max(Highest, Height);
			}
			CHECK(Highest < 150.f, "one kick: the pelvis tops at %.0f cm (it was 4 to 5 m in the video of gate 74)", Highest);
			std::printf("thrown body: pelvis tops at %.0f cm above the ground after the hardest kick (bound %.0f)\n", Highest, B.MaxHeightCm);
		}

		// Lying on a car: slides after 0.4 s, dropped after 1.5 s; a free throw over the car never counts as lying.
		{
			FOnCarState S;
			EOnCar Last = EOnCar::No;
			float SlideAt = -1.f, DropAt = -1.f;
			for (float T = 0.f; T < 3.f; T += 1.f / 60.f)
			{
				Last = StepOnCar(S, 100.f, 0.f, 1.f / 60.f, B);
				if (Last == EOnCar::Slide && SlideAt < 0.f) SlideAt = T;
				if (Last == EOnCar::Drop && DropAt < 0.f) DropAt = T;
			}
			CHECK(SlideAt > 1.1f && SlideAt < 1.3f, "slides off %.2f s after the hit", SlideAt);
			CHECK(DropAt > 2.2f && DropAt < 2.4f, "dropped %.2f s after the hit", DropAt);
			// A person who takes his time to go down (pelvis high, hardly moving) is not lying on a car.
			FOnCarState Slow;
			bool bSlowCounted = false;
			for (float T = 0.f; T < 0.75f; T += 1.f / 60.f)
			{
				bSlowCounted = bSlowCounted || StepOnCar(Slow, 90.f, -20.f, 1.f / 60.f, B) != EOnCar::No;
			}
			CHECK(!bSlowCounted, "still going down is not lying on a car");
			FOnCarState Fly;
			float Height = 95.f, Vz = B.MaxUpCmS;
			bool bCounted = false;
			for (float T = 0.f; T < 2.f && Height > 20.f; T += 1.f / 60.f)
			{
				bCounted = bCounted || StepOnCar(Fly, Height, Vz, 1.f / 60.f, B) != EOnCar::No;
				Height += Vz / 60.f;
				Vz -= 980.f / 60.f;
			}
			CHECK(!bCounted, "a free throw is not a body lying on a car");
			FOnCarState Ground;
			for (int I = 0; I < 600; ++I)
			{
				CHECK(StepOnCar(Ground, 20.f, 0.f, 1.f / 60.f, B) == EOnCar::No, "a body on the ground is not on a car");
			}
			FOnCarState Again;
			for (int I = 0; I < 70; ++I) StepOnCar(Again, 100.f, 0.f, 1.f / 60.f, B);
			CHECK(Again.Seconds > 0.f, "counting once the grace is over");
			StepOnCar(Again, 20.f, 0.f, 1.f / 60.f, B);
			CHECK(Again.Seconds == 0.f, "back on the ground, the count starts again");
		}

		// When a car reaches a person. A 4 m car, 1.8 m wide, going ahead at 40 km/h (lead 56 cm at 0.05 s).
		{
			const float HL = 200.f, HW = 90.f;
			CHECK(!CarReaches(400.f, 0.f, HL, HW, Car40), "2 m ahead of the bumper: not yet");
			CHECK(!CarReaches(300.f, 0.f, HL, HW, Car40), "1 m ahead at 40 km/h: not yet");
			CHECK(CarReaches(270.f, 0.f, HL, HW, Car40), "70 cm ahead at 40 km/h: now, before the bumper is in him");
			CHECK(CarReaches(210.f, 100.f, HL, HW, Car40), "at the corner of the bumper");
			CHECK(!CarReaches(210.f, 130.f, HL, HW, Car40), "40 cm beside the corner: missed");
			CHECK(CarReaches(0.f, 95.f, HL, HW, Car40), "brushed by the flank");
			CHECK(!CarReaches(0.f, 110.f, HL, HW, Car40), "20 cm from the flank: missed (the old circle of 170 cm took him)");
			CHECK(!CarReaches(-230.f, 0.f, HL, HW, Car40), "behind a car going ahead");
			CHECK(CarReaches(-230.f, 0.f, HL, HW, -Car40), "behind a car backing at speed");
			CHECK(!CarReaches(260.f, 0.f, HL, HW, -Car40), "60 cm in front of a car backing away");
			// The old rule let the car's bumper go 30 cm into the person before he fell; the new one never does.
			bool bBefore = true;
			for (float Kmh = 18.f; Kmh <= 130.f; Kmh += 1.f)
			{
				const float Speed = CmSFromKmh(Kmh);
				// Walk the car up to a standing person at 30 and 60 frames a second, from several starts, and note
				// where he is when it reaches him.
				for (const float Fps : {30.f, 60.f})
				{
					for (float Start = 800.f; Start < 900.f; Start += 7.f)
					{
						for (float Along = Start; Along > 0.f; Along -= Speed / Fps)
						{
							if (CarReaches(Along, 0.f, HL, HW, Speed))
							{
								bBefore = bBefore && Along >= HL;
								break;
							}
						}
					}
				}
			}
			CHECK(bBefore, "from 18 to 130 km/h, at 30 and 60 frames a second, the person falls before the bumper is inside him");
		}
	}

	std::printf(Failures == 0 ? "PASS impact logic\n" : "FAILED %d checks\n", Failures);
	return Failures == 0 ? 0 : 1;
}
