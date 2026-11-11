#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPDriveLogic.h"
#include "GTPTestDrive.generated.h"

class AGTPVehicle;
class APlayerController;

/**
 * Replays a named test drive: the player's car follows a timed path written by
 * tools/test/drive_route.py (t_s,x_cm,y_cm,speed_kmh per row), so a measurement
 * or a video drives the same streets at the same speed on every build.
 *
 * Command line (with -SkipTitle; add -TestVehicle on a map with no parked car):
 *   -TestDrive=<name>        Content/Test/Drives/<name>.csv (staged with the game),
 *                            or a full path to a CSV (no spaces).
 *   -TestDriveMode=rail      default. The car is carried along the path by game
 *                            time, physics off: the same camera path on every run,
 *                            whatever the frame rate or the traffic. For benches.
 *   -TestDriveMode=physics   the car is driven with its real pedals and steering
 *                            (GTPDriveLogic.h, as the AI drivers), stops for what
 *                            is ahead and backs off when stuck. For gameplay tests.
 *   -TestDriveLoop           start again from the first point at the end.
 *   -TestDriveQuit           close the game at the end of the drive.
 *   -TestDriveDelay=<s>      game seconds in the car before moving (default 3).
 *
 * The player walks nowhere: the car is placed at the first point facing the
 * second, the character next to its driver door, and gets in through the normal
 * entry. Every 5 s of drive time the position is logged as
 * "DemoPos t=... x=... y=... yaw=... kmh=... phase=testdrive" (same format as the
 * deterministic demo, tools/perf/compare_demo_positions.py compares two runs);
 * "TestDrive end" marks the end. Not used together with -DemoRoute.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPTestDrive : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** One row of the path. */
	struct FPoint
	{
		double T = 0.0;
		FVector2D Location = FVector2D::ZeroVector;
		float SpeedKmh = 0.f;
	};

	/** Reads a path CSV (header t_s,x_cm,y_cm,speed_kmh); empty on error. */
	static TArray<FPoint> LoadPath(const FString& File);

	/** Position and heading (degrees) at drive time T, clamped to the path. */
	static void Sample(const TArray<FPoint>& Path, double T, FVector2D& OutLocation, float& OutYaw, float& OutSpeedKmh);

private:
	enum class EPhase : uint8
	{
		WaitForPawn,
		Place,
		Enter,
		Hold,
		Drive,
		Done
	};

	bool Tick(float DeltaTime);
	void SetPhase(EPhase NewPhase);
	double GameTime() const;
	APlayerController* GetController() const;
	AGTPVehicle* FindCar() const;
	float GroundZ(const AGTPVehicle* InCar, const FVector2D& At, float FallbackZ) const;
	void StepRail(AGTPVehicle* InCar, double T);
	void StepPhysics(AGTPVehicle* InCar, double T);
	void PutAtStart(AGTPVehicle* InCar);
	void LogPosition(const AGTPVehicle* InCar, double T);

	FTSTicker::FDelegateHandle TickHandle;
	TArray<FPoint> Path;
	EPhase Phase = EPhase::WaitForPawn;
	double PhaseStart = 0.0;
	double DriveStart = 0.0;
	bool bRail = true;
	bool bLoop = false;
	bool bQuit = false;
	double Delay = 3.0;
	int32 LastLoggedSlot = -1;
	float LastZ = 0.f;
	TWeakObjectPtr<AGTPVehicle> Car;
	// Physics mode: the path in metres for GTPDrive::Decide.
	std::vector<FGTPDrivePoint> DrivePoints;
	int32 Closest = 0;
	double StuckSince = -1.0;
	double UnstickUntil = -1.0;
	double OffPathSince = -1.0;
	FVector LastLocation = FVector::ZeroVector;
	float UnstickSteer = 0.f;
};
