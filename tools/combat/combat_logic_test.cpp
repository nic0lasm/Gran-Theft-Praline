// Checks the combat rules (the headers the game compiles): health and armour,
// damage and impacts, the pistol, the carbine and the loadout, drive-by, aim assist, bare-hands fighting and cover.
// No Unreal. ADR-0011.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public
//       tools/combat/combat_logic_test.cpp -o /tmp/combat_logic_test && /tmp/combat_logic_test
#include "GTPCombatMeleeLogic.h"
#include "GTPCoverLogic.h"
#include "GTPDamageLogic.h"
#include "GTPHealthLogic.h"
#include "GTPSurfaceLogic.h"
#include "GTPWeaponLogic.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
static int Checks = 0;
#define CHECK(Cond) do { ++Checks; if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

static bool Near(float A, float B, float Eps = 1e-3f) { return std::fabs(A - B) <= Eps; }

static void TestHealth()
{
	using namespace GTPHealth;
	const FSettings S;
	FState H = MakeState(S, 50.f);
	CHECK(H.Health == 100.f && H.Armor == 50.f);

	// Armour takes 70 % of a hit while it lasts.
	FDamageResult R = ApplyDamage(H, S, 20.f);
	CHECK(Near(R.ArmorLost, 14.f) && Near(R.HealthLost, 6.f) && !R.bKilled);
	CHECK(Near(H.Armor, 36.f) && Near(H.Health, 94.f));
	// Ignoring armour (fall, melee) goes straight to health.
	R = ApplyDamage(H, S, 10.f, true);
	CHECK(Near(R.ArmorLost, 0.f) && Near(H.Health, 84.f) && Near(H.Armor, 36.f));
	// Armour runs out mid-hit: the rest hurts.
	H.Armor = 5.f;
	R = ApplyDamage(H, S, 30.f);
	CHECK(Near(R.ArmorLost, 5.f) && Near(R.HealthLost, 25.f) && Near(H.Health, 59.f));

	// Regeneration waits, then stops at half health.
	FState G = MakeState(S);
	ApplyDamage(G, S, 80.f);
	CHECK(Near(G.Health, 20.f));
	for (int I = 0; I < 50; ++I) Tick(G, S, 0.1f); // 5 s: still waiting
	CHECK(Near(G.Health, 20.f));
	for (int I = 0; I < 200; ++I) Tick(G, S, 0.1f); // 20 s more
	CHECK(Near(G.Health, 50.f));
	// A new hit restarts the wait.
	ApplyDamage(G, S, 10.f);
	for (int I = 0; I < 30; ++I) Tick(G, S, 0.1f);
	CHECK(Near(G.Health, 40.f));
	// Healing is not capped by the regeneration cap.
	CHECK(Near(Heal(G, S, 100.f), 60.f) && Near(G.Health, 100.f));

	// Stamina: refused when short, refills after a pause.
	FState St = MakeState(S);
	CHECK(SpendStamina(St, 60.f) && !SpendStamina(St, 60.f) && Near(St.Stamina, 40.f));
	for (int I = 0; I < 30; ++I) Tick(St, S, 0.1f);
	CHECK(St.Stamina > 40.f && St.Stamina <= 100.f);

	// Death, ignored hits once dead, respawn timer fires once, grace protects.
	FState D = MakeState(S);
	R = ApplyDamage(D, S, 150.f);
	CHECK(R.bKilled && D.bDead && D.Health == 0.f);
	CHECK(ApplyDamage(D, S, 10.f).bIgnored);
	int Fired = 0;
	for (int I = 0; I < 100; ++I) Fired += Tick(D, S, 0.1f) ? 1 : 0;
	CHECK(Fired == 1);
	Respawn(D, S);
	CHECK(!D.bDead && Near(D.Health, 100.f) && D.Armor == 0.f);
	CHECK(ApplyDamage(D, S, 10.f).bIgnored);
	for (int I = 0; I < 40; ++I) Tick(D, S, 0.1f);
	CHECK(!ApplyDamage(D, S, 10.f).bIgnored);

	// Nearest hospital.
	std::vector<FHospital> Hs = {{0.f, 0.f, 0.f, 0.f}, {100000.f, 0.f, 0.f, 0.f}, {50000.f, 50000.f, 0.f, 0.f}};
	CHECK(PickHospital(Hs, 90000.f, 10000.f) == 1);
	CHECK(PickHospital(Hs, 40000.f, 60000.f) == 2);
	CHECK(PickHospital({}, 0.f, 0.f) == -1);
}

static void TestDamage()
{
	using namespace GTPDamage;
	CHECK(ZoneFromBone("head") == EZone::Head);
	CHECK(ZoneFromBone("neck_01") == EZone::Head);
	CHECK(ZoneFromBone("spine_03") == EZone::Torso);
	CHECK(ZoneFromBone("pelvis") == EZone::Torso);
	CHECK(ZoneFromBone("upperarm_l") == EZone::Arm);
	CHECK(ZoneFromBone("hand_r") == EZone::Arm);
	CHECK(ZoneFromBone("index_02_r") == EZone::Arm);
	CHECK(ZoneFromBone("thigh_r") == EZone::Leg);
	CHECK(ZoneFromBone("calf_twist_01_l") == EZone::Leg);
	CHECK(ZoneFromBone("ball_l") == EZone::Leg);
	CHECK(ZoneFromBone("") == EZone::Torso && ZoneFromBone(nullptr) == EZone::Torso);

	const FWeaponDamage W;
	CHECK(Near(FalloffFactor(W, 5.f), 1.f));
	CHECK(Near(FalloffFactor(W, 30.f), 0.75f));
	CHECK(Near(FalloffFactor(W, 100.f), 0.5f));
	CHECK(Near(ComputeDamage(W, EZone::Torso, 10.f, false), 20.f));
	CHECK(Near(ComputeDamage(W, EZone::Head, 10.f, false), 50.f));
	CHECK(Near(ComputeDamage(W, EZone::Leg, 10.f, false), 14.f));
	// The player takes 45 %, and a head shot counts at most 1.5 times.
	CHECK(Near(ComputeDamage(W, EZone::Torso, 10.f, true), 9.f));
	CHECK(Near(ComputeDamage(W, EZone::Head, 10.f, true), 13.5f));

	// Surface ids match the footstep map.
	CHECK(SurfaceId::Metal == GTPSurfaceId::Metal && SurfaceId::Wood == GTPSurfaceId::Wood);
	CHECK(SurfaceId::Water == GTPSurfaceId::Water && SurfaceId::Grass == GTPSurfaceId::Grass);
	CHECK(SurfaceId::Gravel == GTPSurfaceId::Gravel && SurfaceId::Cobble == GTPSurfaceId::Cobble);
	FImpactContext C;
	CHECK(ImpactFor(C) == EImpact::Dust);
	C.SurfaceId = SurfaceId::Metal;
	CHECK(ImpactFor(C) == EImpact::Sparks);
	C.bHitVehicle = true;
	CHECK(ImpactFor(C) == EImpact::CarBody);
	C.bGlass = true;
	CHECK(ImpactFor(C) == EImpact::Glass);
	C.bHitPawn = true;
	CHECK(ImpactFor(C) == EImpact::Flesh);
	FImpactContext Wd;
	Wd.SurfaceId = SurfaceId::Water;
	CHECK(ImpactFor(Wd) == EImpact::Water);
	for (int I = 0; I < static_cast<int>(EImpact::Count); ++I)
	{
		CHECK(MaxParticles(static_cast<EImpact>(I)) > 0 && MaxParticles(static_cast<EImpact>(I)) <= 48);
	}
}

static void TestPistol()
{
	using namespace GTPWeapon;
	const FSpec S;
	FState W = MakeState(S, 36);
	FRandom Rng(1234);
	FShot Shot;
	CHECK(W.Magazine == 12 && W.Reserve == 36);

	// Semi-automatic: one shot per press, then the fire rate.
	CHECK(PullTrigger(W, S, true, 0.f, false, Rng, Shot) == EFire::Fired);
	CHECK(PullTrigger(W, S, true, 0.f, false, Rng, Shot) == EFire::NeedsRelease);
	ReleaseTrigger(W);
	CHECK(PullTrigger(W, S, true, 0.f, false, Rng, Shot) == EFire::Cooldown);
	ReleaseTrigger(W);
	Tick(W, S, 0.2f);
	CHECK(PullTrigger(W, S, true, 0.f, false, Rng, Shot) == EFire::Fired);
	CHECK(W.Magazine == 10);

	// Every shot stays inside the cone of the moment.
	FState C = MakeState(S, 96);
	FRandom R2(42);
	bool bInside = true;
	for (int I = 0; I < 200; ++I)
	{
		const float Cone = SpreadDeg(C, S, false, 0.5f);
		FShot Sh;
		ReleaseTrigger(C);
		C.Cooldown = 0.f;
		if (C.Magazine == 0 || C.IsReloading()) { C.ReloadLeft = 0.f; C.Magazine = 12; }
		if (PullTrigger(C, S, false, 0.5f, false, R2, Sh) == EFire::Fired)
		{
			bInside = bInside && std::sqrt(Sh.YawDeg * Sh.YawDeg + Sh.PitchDeg * Sh.PitchDeg) <= Cone + 1e-4f;
		}
	}
	CHECK(bInside);

	// Bloom grows, is capped and recovers; aiming is tighter than the hip; blind fire is wide.
	FState B = MakeState(S, 0);
	const float Aim0 = SpreadDeg(B, S, true, 0.f);
	CHECK(Aim0 < SpreadDeg(B, S, false, 0.f));
	CHECK(SpreadDeg(B, S, true, 0.f, true) > 3.f * Aim0);
	for (int I = 0; I < 10; ++I) { ReleaseTrigger(B); B.Cooldown = 0.f; FShot Sh; PullTrigger(B, S, true, 0.f, false, Rng, Sh); }
	CHECK(Near(B.BloomDeg, S.MaxBloomDeg));
	Tick(B, S, 2.f);
	CHECK(Near(B.BloomDeg, 0.f));

	// Recoil adds up and comes back fully.
	FState K = MakeState(S, 0);
	for (int I = 0; I < 3; ++I) { ReleaseTrigger(K); K.Cooldown = 0.f; FShot Sh; PullTrigger(K, S, true, 0.f, false, Rng, Sh); }
	CHECK(Near(K.RecoilPitchDeg, 3.f * S.RecoilPitchDeg));
	float Back = 0.f;
	for (int I = 0; I < 100; ++I) Back += Tick(K, S, 0.05f);
	CHECK(Near(Back, 3.f * S.RecoilPitchDeg) && Near(K.RecoilPitchDeg, 0.f));

	// Emptying the magazine starts the reload; the magazine fills only at the end.
	FState E = MakeState(S, 20);
	for (int I = 0; I < 12; ++I) { ReleaseTrigger(E); E.Cooldown = 0.f; FShot Sh; CHECK(PullTrigger(E, S, true, 0.f, false, Rng, Sh) == EFire::Fired); }
	CHECK(E.Magazine == 0 && E.IsReloading());
	ReleaseTrigger(E);
	CHECK(PullTrigger(E, S, true, 0.f, false, Rng, Shot) == EFire::Reloading);
	Tick(E, S, 1.0f);
	CHECK(E.Magazine == 0);
	Tick(E, S, 0.6f);
	CHECK(E.Magazine == 12 && E.Reserve == 8 && !E.IsReloading());

	// A cancelled reload loses nothing and fills nothing.
	FState X = MakeState(S, 30);
	X.Magazine = 3;
	CHECK(StartReload(X, S));
	Tick(X, S, 0.5f);
	CancelReload(X);
	Tick(X, S, 2.f);
	CHECK(X.Magazine == 3 && X.Reserve == 30);
	// A full magazine or an empty reserve does not reload.
	FState F = MakeState(S, 30);
	CHECK(!StartReload(F, S));
	FState Z = MakeState(S, 0);
	Z.Magazine = 0;
	ReleaseTrigger(Z);
	CHECK(PullTrigger(Z, S, true, 0.f, false, Rng, Shot) == EFire::Empty);
	// Last magazine: the reload takes what is left.
	FState L = MakeState(S, 5);
	L.Magazine = 2;
	StartReload(L, S);
	Tick(L, S, 2.f);
	CHECK(L.Magazine == 7 && L.Reserve == 0);
	// Ammunition pick-up is capped.
	CHECK(AddAmmo(L, S, 500) == S.MaxReserve && L.Reserve == S.MaxReserve);

	// Same seed, same shots (replayed fights).
	FRandom A(7), Bq(7);
	FState Sa = MakeState(S, 0), Sb = MakeState(S, 0);
	FShot Ha, Hb;
	PullTrigger(Sa, S, false, 0.3f, false, A, Ha);
	PullTrigger(Sb, S, false, 0.3f, false, Bq, Hb);
	CHECK(Ha.YawDeg == Hb.YawDeg && Ha.PitchDeg == Hb.PitchDeg && Ha.KickYawDeg == Hb.KickYawDeg);
}

static void TestAimAssist()
{
	using namespace GTPAimAssist;
	std::vector<FTarget> T(4);
	T[0].AngleDeg = 6.f; T[0].DistanceM = 10.f;
	T[1].AngleDeg = 2.f; T[1].DistanceM = 35.f;
	T[2].AngleDeg = 1.f; T[2].DistanceM = 5.f; T[2].bExcluded = true; // surrendered
	T[3].AngleDeg = 20.f; T[3].DistanceM = 3.f;                      // outside the cone
	// 6 + 1.5 = 7.5 against 2 + 5.25 = 7.25: the far one, nearly centred, wins.
	CHECK(PickSnapTarget(T) == 1);
	T[0].bHostile = true; // 7.5 - 3 = 4.5
	CHECK(PickSnapTarget(T) == 0);
	T[0].bVisible = false;
	CHECK(PickSnapTarget(T) == 1);
	T[1].DistanceM = 60.f;
	CHECK(PickSnapTarget(T) == -1);
	CHECK(Near(Friction(0.f), 0.45f) && Near(Friction(10.f), 1.f));
	CHECK(Friction(1.f) > Friction(0.5f) && Friction(1.f) < 1.f);
}

static void TestArsenal()
{
	using namespace GTPArsenal;
	// The carbine holds the trigger: three shots in 0.3 s without releasing it.
	{
		const GTPWeapon::FSpec C = Carbine();
		GTPWeapon::FState St = GTPWeapon::MakeState(C, 60);
		GTPWeapon::FRandom Rng(7u);
		GTPWeapon::FShot Shot;
		int Fired = 0;
		for (int I = 0; I < 30; ++I)
		{
			if (GTPWeapon::PullTrigger(St, C, true, 0.f, false, Rng, Shot) == GTPWeapon::EFire::Fired) ++Fired;
			GTPWeapon::Tick(St, C, 0.01f);
		}
		CHECK(Fired == 3);
		CHECK(St.Magazine == 27);
		// Recoil climbs while held, then comes back.
		CHECK(St.RecoilPitchDeg > 0.9f);
		for (int I = 0; I < 100; ++I) GTPWeapon::Tick(St, C, 0.01f);
		CHECK(Near(St.RecoilPitchDeg, 0.f));
		CHECK(!C.bOneHanded && Pistol().bOneHanded);
		// Officer (100 health): 4 carbine torso shots at 10 m, 2 to the head.
		GTPHealth::FSettings HS;
		GTPHealth::FState Cop = GTPHealth::MakeState(HS);
		int Shots = 0;
		while (!Cop.bDead && Shots < 20) { GTPHealth::ApplyDamage(Cop, HS, GTPDamage::ComputeDamage(C.Damage, GTPDamage::EZone::Torso, 10.f, false)); ++Shots; }
		CHECK(Shots == 4);
	}
	// Loadout: pistol first, switch, magazines kept, reload stopped by the switch.
	{
		FLoadout L = FLoadout::Make({EKind::Pistol, EKind::Carbine}, {36, 90});
		CHECK(L.CurrentKind() == EKind::Pistol && L.Has(EKind::Carbine));
		CHECK(L.State().Reserve == 36 && L.States[1].Reserve == 90 && L.States[1].Magazine == 30);
		L.State().Magazine = 3;
		CHECK(GTPWeapon::StartReload(L.State(), L.Spec()));
		L.State().bRightShoulder = false;
		CHECK(L.Next() == EKind::Carbine && L.Next(-1) == EKind::Carbine);
		CHECK(L.Select(EKind::Carbine, 0.4f));
		CHECK(L.IsSwitching() && L.CurrentKind() == EKind::Carbine);
		CHECK(!L.State().bRightShoulder);            // the shoulder follows
		CHECK(!L.States[0].IsReloading() && L.States[0].Magazine == 3); // pistol reload stopped, rounds kept
		CHECK(!L.TickSwitch(0.2f) && L.TickSwitch(0.25f) && !L.IsSwitching());
		CHECK(!L.Select(EKind::Carbine, 0.4f));      // already in hand
		CHECK(L.FirstOneHanded() == EKind::Pistol);
		// Pistol only: Next stays, no carbine to select, Give adds it once.
		FLoadout P = FLoadout::Make({EKind::Pistol}, {24});
		CHECK(P.Next() == EKind::Pistol && !P.Select(EKind::Carbine, 0.f));
		CHECK(P.Give(EKind::Carbine, 60) && !P.Give(EKind::Carbine, 30) && P.States[1].Reserve == 90);
		// Picking up: a new weapon comes with its rounds, a carried one tops up its reserve.
		FLoadout Q = FLoadout::Make({EKind::Pistol}, {90});
		CHECK(Q.Collect(EKind::Pistol, 20) == 6 && Q.States[0].Reserve == 96); // capped at 96
		CHECK(Q.Collect(EKind::Pistol, 20) == 0);
		CHECK(Q.Collect(EKind::Carbine, 40) == 40 && Q.Has(EKind::Carbine) && Q.States[1].Magazine == 30);
		CHECK(Q.Collect(EKind::Carbine, 0) == 0);
		// Dropped guns: magazine + half the reserve, at least 6, at most two magazines.
		CHECK(GTPPickup::DropRounds(12, 36, 12) == 24 && GTPPickup::DropRounds(5, 4, 12) == 7);
		CHECK(GTPPickup::DropRounds(0, 0, 12) == 6 && GTPPickup::DropRounds(30, 180, 30) == 60);
		// Disarmed: killed while carrying, or a heavy blow / takedown while the gun is in hand.
		CHECK(GTPPickup::Disarms(false, false, true, true, false));
		CHECK(GTPPickup::Disarms(true, true, false, true, true));
		CHECK(!GTPPickup::Disarms(true, true, false, true, false));  // holstered: kept
		CHECK(!GTPPickup::Disarms(true, false, false, true, true));  // light blow: kept
		CHECK(!GTPPickup::Disarms(false, false, false, true, true)); // shot, alive: kept
		CHECK(!GTPPickup::Disarms(false, false, true, false, false)); // nothing to drop
		P.StripAll();
		CHECK(P.States[0].Magazine == 0 && P.States[0].Reserve == 0 && P.States[1].Reserve == 0 && P.Has(EKind::Carbine));
		FLoadout Long = FLoadout::Make({EKind::Carbine}, {60});
		CHECK(Long.FirstOneHanded() == EKind::Count);
	}
	// Drive-by: sides and windscreen yes, straight back no; spread grows with speed.
	{
		using namespace GTPDriveBy;
		CHECK(CanShoot(-90.f, 0.f) && CanShoot(90.f, 10.f) && CanShoot(0.f, -5.f));
		CHECK(!CanShoot(175.f, 0.f) && !CanShoot(-160.f, 0.f) && !CanShoot(90.f, 60.f) && !CanShoot(90.f, -45.f));
		CHECK(Near(SpeedSpreadDeg(0.f), 0.f) && Near(SpeedSpreadDeg(15.f), 1.25f) && Near(SpeedSpreadDeg(80.f), 2.5f));
		CHECK(WindowFor(-90.f) == EWindow::Left && WindowFor(10.f) == EWindow::Windscreen && WindowFor(120.f) == EWindow::Right);
	}
}

static void TestMelee()
{
	using namespace GTPMelee;
	const FSettings S;
	float Spent = 0.f;
	float Stamina = 100.f;

	// One blow: windup, lands once, recovers, back to idle.
	FFighter F(S);
	F.Input(EInput::Attack, Stamina, Spent);
	CHECK(F.GetPhase() == EPhase::Windup && Near(Spent, S.StaminaPerBlow));
	int Lands = 0;
	for (int I = 0; I < 60; ++I)
	{
		FStep St = F.Tick(1.f / 60.f, Stamina, Spent);
		if (St.bBlowLands) { ++Lands; CHECK(St.BlowIndex == 0 && Near(St.BlowDamage, 8.f) && !St.bHeavy); }
	}
	CHECK(Lands == 1 && F.GetPhase() == EPhase::Idle);

	// Combo: pressing during recovery chains three blows, the third is heavy.
	FFighter C(S);
	C.Input(EInput::Attack, Stamina, Spent);
	int Blows = 0;
	bool bHeavySeen = false;
	for (int I = 0; I < 180; ++I)
	{
		if (C.GetPhase() == EPhase::Recovery) C.Input(EInput::Attack, Stamina, Spent);
		FStep St = C.Tick(1.f / 60.f, Stamina, Spent);
		if (St.bBlowLands) { ++Blows; bHeavySeen = bHeavySeen || St.bHeavy; CHECK(St.BlowIndex == Blows - 1); }
	}
	CHECK(Blows == 3 && bHeavySeen && C.GetPhase() == EPhase::Idle);

	// Not enough stamina: no blow.
	FFighter Tired(S);
	Tired.Input(EInput::Attack, 3.f, Spent);
	CHECK(Tired.GetPhase() == EPhase::Idle && Spent == 0.f);

	// Dodge: invulnerable only in the middle of it.
	FFighter D(S);
	D.Input(EInput::Dodge, Stamina, Spent);
	CHECK(D.GetPhase() == EPhase::Dodge && Near(Spent, S.DodgeStamina));
	CHECK(!D.IsInvulnerable());
	D.Tick(0.1f, Stamina, Spent);
	CHECK(D.IsInvulnerable() && D.Receive(10.f, false, 0.f, Stamina).Outcome == EReceive::Dodged);
	D.Tick(0.25f, Stamina, Spent);
	CHECK(!D.IsInvulnerable() && D.Receive(10.f, false, 0.f, Stamina).Outcome == EReceive::Hit);

	// Guard: stops 80 % of a frontal blow for stamina, not one from behind.
	FFighter G(S);
	G.Input(EInput::GuardOn, Stamina, Spent);
	CHECK(G.GetPhase() == EPhase::Guard);
	FReceiveResult R = G.Receive(10.f, false, 20.f, Stamina);
	CHECK(R.Outcome == EReceive::Blocked && Near(R.Damage, 2.f) && Near(R.StaminaCost, S.GuardStaminaPerBlock));
	R = G.Receive(10.f, false, 150.f, Stamina);
	CHECK(R.Outcome == EReceive::Hit && Near(R.Damage, 10.f));
	// Out of stamina, the guard breaks and stuns; the stun wears off back into the guard.
	R = G.Receive(10.f, false, 0.f, 5.f);
	CHECK(R.Outcome == EReceive::GuardBroken && G.GetPhase() == EPhase::Stunned);
	for (int I = 0; I < 90; ++I) G.Tick(1.f / 60.f, Stamina, Spent);
	CHECK(G.GetPhase() == EPhase::Guard);
	// A heavy blow breaks a guard even with stamina.
	CHECK(G.Receive(18.f, true, 0.f, Stamina).Outcome == EReceive::GuardBroken);

	// A blow during the windup interrupts the attack; a heavy one knocks down.
	FFighter W(S);
	W.Input(EInput::Attack, Stamina, Spent);
	R = W.Receive(8.f, false, 0.f, Stamina);
	CHECK(R.bInterrupted && W.GetPhase() == EPhase::Stunned && !R.bKnockdown);
	FFighter H(S);
	CHECK(H.Receive(18.f, true, 0.f, Stamina).bKnockdown);

	// Takedowns.
	CHECK(CanTakedown(S, 100.f, 10.f, false, false, false));   // unaware, from behind
	CHECK(!CanTakedown(S, 100.f, 10.f, false, true, false));   // aware
	CHECK(!CanTakedown(S, 100.f, 150.f, false, false, false)); // from the front
	CHECK(CanTakedown(S, 100.f, 150.f, true, true, false));    // stunned: any side
	CHECK(!CanTakedown(S, 300.f, 0.f, true, false, false));    // too far
	CHECK(!CanTakedown(S, 100.f, 0.f, false, false, true));    // armed and aiming
}

static void TestCover()
{
	using namespace GTPCover;
	FProbe Wall;
	Wall.KneeDistCm = Wall.ChestDistCm = Wall.HeadDistCm = 60.f;
	CHECK(Classify(Wall) == EType::High);
	FProbe Car;
	Car.KneeDistCm = 50.f; Car.ChestDistCm = 70.f;
	CHECK(Classify(Car) == EType::Low);
	FProbe Planter;
	Planter.KneeDistCm = 40.f;
	CHECK(Classify(Planter) == EType::Low);
	FProbe Far = Wall;
	Far.KneeDistCm = Far.ChestDistCm = Far.HeadDistCm = 300.f;
	CHECK(Classify(Far) == EType::None);
	FProbe Oblique = Wall;
	Oblique.FacingAngleDeg = 70.f;
	CHECK(Classify(Oblique) == EType::None);
	FProbe Railing; // chest only, open underneath
	Railing.ChestDistCm = 50.f;
	CHECK(Classify(Railing) == EType::None);

	FProbe Corner = Wall;
	Corner.LeftSideDistCm = 30.f; // wall continues on the left
	FEdges E = FindEdges(Corner);
	CHECK(!E.bLeft && E.bRight);
	CHECK(AimFrom(EType::High, E, false) == EAimFrom::LeanRight);
	FEdges None;
	CHECK(AimFrom(EType::High, None, true) == EAimFrom::BlindOnly);
	FEdges Both{true, true};
	CHECK(AimFrom(EType::High, Both, false) == EAimFrom::LeanLeft);
	CHECK(AimFrom(EType::High, Both, true) == EAimFrom::LeanRight);
	CHECK(AimFrom(EType::Low, None, true) == EAimFrom::OverTop);

	// Cover to cover: the point along the stick, not the nearer one behind, not the current one.
	std::vector<FPoint> P(4);
	P[0].X = 50.f;  P[0].Y = 0.f;     // current cover
	P[1].X = 600.f; P[1].Y = 100.f;   // ahead, slightly off
	P[2].X = -400.f; P[2].Y = 0.f;    // behind
	P[3].X = 900.f; P[3].Y = 0.f;     // ahead, straight, farther
	const int Next = PickNextCover(P, 0.f, 0.f, 1.f, 0.f);
	CHECK(Next == 1 || Next == 3);
	CHECK(Next == 1); // 608 + 9.5 x 15 = 750 < 900
	CHECK(PickNextCover(P, 0.f, 0.f, 0.f, 1.f) == -1);
	CHECK(PickNextCover(P, 0.f, 0.f, -1.f, 0.f) == 2);

	// Protection: wall with its normal toward -x protects from shooters at +x.
	FPoint Pt;
	Pt.NormalX = -1.f; Pt.NormalY = 0.f;
	CHECK(IsProtected(Pt, 2000.f, 300.f, false));
	CHECK(!IsProtected(Pt, -2000.f, 0.f, false));
	CHECK(!IsProtected(Pt, 2000.f, 0.f, true)); // leaning out
	CHECK(!IsProtected(Pt, 0.f, 2000.f, false)); // flanked
}

// Time to kill with the default numbers: the balance the ADR promises.
static void TestBalance()
{
	const GTPWeapon::FSpec Pistol;
	const GTPHealth::FSettings HS;
	// The player kills an unarmoured officer at 10 m with 5 torso shots, or 2 to the head.
	{
		GTPHealth::FState Cop = GTPHealth::MakeState(HS);
		int Shots = 0;
		while (!Cop.bDead && Shots < 20) { GTPHealth::ApplyDamage(Cop, HS, GTPDamage::ComputeDamage(Pistol.Damage, GTPDamage::EZone::Torso, 10.f, false)); ++Shots; }
		CHECK(Shots == 5);
		GTPHealth::FState Cop2 = GTPHealth::MakeState(HS);
		Shots = 0;
		while (!Cop2.bDead && Shots < 20) { GTPHealth::ApplyDamage(Cop2, HS, GTPDamage::ComputeDamage(Pistol.Damage, GTPDamage::EZone::Head, 10.f, false)); ++Shots; }
		CHECK(Shots == 2);
	}
	// The player at full health with no armour survives 11 torso hits from the same pistol;
	// with full armour, many more. Never a one-shot.
	{
		GTPHealth::FState P = GTPHealth::MakeState(HS);
		int Hits = 0;
		while (!P.bDead && Hits < 100) { GTPHealth::ApplyDamage(P, HS, GTPDamage::ComputeDamage(Pistol.Damage, GTPDamage::EZone::Torso, 10.f, true)); ++Hits; }
		CHECK(Hits == 12);
		GTPHealth::FState A = GTPHealth::MakeState(HS, 100.f);
		Hits = 0;
		while (!A.bDead && Hits < 100) { GTPHealth::ApplyDamage(A, HS, GTPDamage::ComputeDamage(Pistol.Damage, GTPDamage::EZone::Torso, 10.f, true)); ++Hits; }
		CHECK(Hits >= 20);
		std::printf("balance: officer 5 torso / 2 head shots; player %d hits with full armour, 12 without\n", Hits);
	}
	// Fists: a full three-blow combo does not kill a fresh body (36 points).
	{
		const GTPMelee::FSettings MS;
		CHECK(MS.Damage[0] + MS.Damage[1] + MS.Damage[2] < 50.f);
	}
}

int main()
{
	TestHealth();
	TestDamage();
	TestPistol();
	TestAimAssist();
	TestArsenal();
	TestMelee();
	TestCover();
	TestBalance();
	if (Failures)
	{
		std::printf("%d of %d checks failed\n", Failures, Checks);
		return 1;
	}
	std::printf("combat rules: %d checks passed\n", Checks);
	return 0;
}
