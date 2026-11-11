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
			CHECK(Time > 1.0f && Time < 5.0f, "complying after %.1f s", Time);
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
		// Fleeing temperament runs; frozen never does, even under fire.
		{
			T::FBrain B;
			B.Temperament = T::ETemperament::Fleeing;
			T::EState Target = T::EState::Fleeing;
			Run(B, Aimed, 10.f, &Target);
			CHECK(B.State == T::EState::Fleeing, "a fleeing temperament runs");
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

	std::printf(Failures == 0 ? "PASS impact logic\n" : "FAILED %d checks\n", Failures);
	return Failures == 0 ? 0 : 1;
}
