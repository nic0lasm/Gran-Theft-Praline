#include "Modules/ModuleManager.h"

#include "GTPZoneGraphImport.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPMassSettings, Log, All);

namespace GTPMassModuleLocal
{
	/**
	 * What City Sample's own config sets for MassTraffic's driving and our project lacked (no
	 * DefaultPlugins.ini since candidate 31, no MassTraffic cvar), read on the PC on 20/11:
	 * Config/DefaultPlugins.ini:112, SpeedPIDControllerParams P 0.0007, I 0.00004, window 0.5 s,
	 * D 0.0001 (the code's defaults are 0.5, 0.5, 1 s, 0.5: on an error in cm/s the throttle and the
	 * brake, times SpeedPIDBrakeMultiplier 5, saturate from 2 cm/s off the target speed; the probe of
	 * candidate 49 read "throttle 1.00" and "brake 5.00"); Config/DefaultEngine.ini:222-223,
	 * MassTraffic.SleepCounterThreshold=4 and MassTraffic.LinearSpeedSleepThreshold=6.0 (the code's: 20
	 * frames under 0.001 cm/s). -MassTrafficCodeDefaults leaves the code's values, for a comparison.
	 * Set by name: MassTraffic is an optional plugin, copied out of Git.
	 */
	void ApplyCitySampleDriving(UClass& Class)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("MassTrafficCodeDefaults")))
		{
			UE_LOG(LogGTPMassSettings, Log, TEXT("MassTraffic settings: the code's defaults kept (-MassTrafficCodeDefaults), not City Sample's speed PID and sleep"));
			return;
		}
		UObject* Settings = Class.GetDefaultObject();
		const FStructProperty* Pid = FindFProperty<FStructProperty>(&Class, TEXT("SpeedPIDControllerParams"));
		FString PidReport = TEXT("SpeedPIDControllerParams not found");
		if (Settings && Pid)
		{
			void* Params = Pid->ContainerPtrToValuePtr<void>(Settings);
			// By name (P, I, window, D), else by order for the three gains.
			TArray<FNumericProperty*> Gains;
			FNumericProperty* Window = nullptr;
			for (TFieldIterator<FNumericProperty> It(Pid->Struct); It; ++It)
			{
				if (!It->IsFloatingPoint())
				{
					continue;
				}
				if (It->GetName().Contains(TEXT("Window")) || It->GetName().Contains(TEXT("Time")))
				{
					Window = *It;
				}
				else
				{
					Gains.Add(*It);
				}
			}
			auto Pick = [&Gains](const TCHAR* Word, int32 Order) -> FNumericProperty*
			{
				for (FNumericProperty* Gain : Gains)
				{
					if (Gain->GetName().Contains(Word))
					{
						return Gain;
					}
				}
				return Gains.IsValidIndex(Order) ? Gains[Order] : nullptr;
			};
			struct FSet
			{
				FNumericProperty* Field;
				double Value;
			};
			const FSet Sets[] = {{Pick(TEXT("Proportional"), 0), 0.0007}, {Pick(TEXT("Integral"), 1), 0.00004}, {Window, 0.5}, {Pick(TEXT("Derivative"), 2), 0.0001}};
			PidReport.Reset();
			for (const FSet& Set : Sets)
			{
				if (!Set.Field)
				{
					PidReport += TEXT(" ?=missing");
					continue;
				}
				void* Value = Set.Field->ContainerPtrToValuePtr<void>(Params);
				const double Was = Set.Field->GetFloatingPointPropertyValue(Value);
				Set.Field->SetFloatingPointPropertyValue(Value, Set.Value);
				PidReport += FString::Printf(TEXT(" %s %g->%g"), *Set.Field->GetName(), Was, Set.Field->GetFloatingPointPropertyValue(Value));
			}
		}
		FString SleepReport;
		const TCHAR* CVars[][2] = {{TEXT("MassTraffic.SleepCounterThreshold"), TEXT("4")}, {TEXT("MassTraffic.LinearSpeedSleepThreshold"), TEXT("6.0")}};
		for (const auto& Pair : CVars)
		{
			IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Pair[0]);
			if (!Variable)
			{
				SleepReport += FString::Printf(TEXT(" %s not found;"), Pair[0]);
				continue;
			}
			const FString Was = Variable->GetString();
			Variable->Set(Pair[1], ECVF_SetByProjectSetting);
			SleepReport += FString::Printf(TEXT(" %s %s->%s;"), Pair[0], *Was, *Variable->GetString());
		}
		UE_LOG(LogGTPMassSettings, Log, TEXT("MassTraffic settings: City Sample's speed PID set:%s | sleep:%s"), *PidReport, *SleepReport);
	}

	/**
	 * MassTraffic gives its simple physics (mined from PhysicsVehicleTemplateActor) to the cars of
	 * simulation LOD High or Medium: by its code's defaults every car within 200 m of the player, up
	 * to 150 (UMassTrafficVehicleSimulationLODProcessor). Our near cars are never spawned as actors,
	 * so that physics brings the player nothing he can touch, and it is what stops the traffic: with
	 * the real wheels and City Sample's speed PID (gate 54), 54 of the 55 cars under it stood in
	 * queues at 265 s while the cars on their lanes drove, and lane [0/5333]'s list of cars broke.
	 * By default no car is given it now: the Medium and Low bands start at 0 m, every car short of the
	 * Off distance is Low and drives on its lane, as the cars beyond 200 m always did.
	 * -MassTrafficPhysicsRadius=<m> gives it back within that distance (200 = the code's).
	 */
	void ApplyPhysicsRadius()
	{
		UClass* Lod = FindObject<UClass>(nullptr, TEXT("/Script/MassTraffic.MassTrafficVehicleSimulationLODProcessor"));
		UObject* Defaults = Lod ? Lod->GetDefaultObject() : nullptr;
		if (!Defaults)
		{
			UE_LOG(LogGTPMassSettings, Warning, TEXT("MassTraffic physics radius: MassTrafficVehicleSimulationLODProcessor not found, left as is"));
			return;
		}
		float Metres = 0.f;
		const bool bAsked = FParse::Value(FCommandLine::Get(), TEXT("MassTrafficPhysicsRadius="), Metres);
		const float Radius = FMath::Max(Metres, 0.f) * 100.f;
		FString Report;
		const TCHAR* Names[] = {TEXT("BaseLODDistance"), TEXT("VisibleLODDistance")};
		for (const TCHAR* Name : Names)
		{
			// EMassLOD: High 0, Medium 1, Low 2, Off 3; each value is where that LOD starts. A car is
			// under the physics while its LOD is High or Medium, so up to where Low starts.
			const FFloatProperty* Starts = FindFProperty<FFloatProperty>(Lod, Name);
			if (!Starts || Starts->ArrayDim < 4)
			{
				Report += FString::Printf(TEXT(" %s not found;"), Name);
				continue;
			}
			float* Medium = Starts->ContainerPtrToValuePtr<float>(Defaults, 1);
			float* Low = Starts->ContainerPtrToValuePtr<float>(Defaults, 2);
			const float Off = *Starts->ContainerPtrToValuePtr<float>(Defaults, 3);
			const float WasMedium = *Medium;
			const float WasLow = *Low;
			const float Edge = FMath::Min(Radius, Off);
			*Medium = Edge;
			*Low = Edge;
			Report += FString::Printf(TEXT(" %s Medium %.0f->%.0f m, Low %.0f->%.0f m, Off %.0f m;"), Name, WasMedium / 100.f, *Medium / 100.f,
				WasLow / 100.f, *Low / 100.f, Off / 100.f);
		}
		UE_LOG(LogGTPMassSettings, Log, TEXT("MassTraffic physics radius: %s within %.0f m of the player (%s) |%s"),
			Radius > 0.f ? TEXT("simple physics") : TEXT("no car under MassTraffic's simple physics, every car on its lane"), Radius / 100.f,
			bAsked ? TEXT("-MassTrafficPhysicsRadius") : TEXT("default, -MassTrafficPhysicsRadius=200 for the code's"), *Report);
	}

	/**
	 * A car of LOD Low moves only when its chunk ticks (UMassTrafficInterpolationProcessor), at the
	 * rate its entity config gives that LOD: what the cars near the player get now that they are Low.
	 * Read once per game world, by name, from the vehicle simulation traits loaded with the map.
	 */
	void LogVehicleTickRates(UWorld* World)
	{
		UClass* Trait = FindObject<UClass>(nullptr, TEXT("/Script/MassTraffic.MassTrafficVehicleSimulationTrait"));
		if (!World || !World->IsGameWorld() || !Trait)
		{
			return;
		}
		const FStructProperty* Tick = FindFProperty<FStructProperty>(Trait, TEXT("VariableTickParams"));
		const FFloatProperty* Rates = Tick ? FindFProperty<FFloatProperty>(Tick->Struct, TEXT("TickRates")) : nullptr;
		if (!Rates)
		{
			UE_LOG(LogGTPMassSettings, Log, TEXT("MassTraffic tick rates: VariableTickParams.TickRates not found"));
			return;
		}
		TArray<UObject*> Traits;
		GetObjectsOfClass(Trait, Traits, true, RF_ClassDefaultObject);
		TMap<FString, int32> Seen;
		for (UObject* Object : Traits)
		{
			void* Params = Tick->ContainerPtrToValuePtr<void>(Object);
			FString Key;
			for (int32 Index = 0; Index < Rates->ArrayDim; ++Index)
			{
				Key += FString::Printf(TEXT("%s%.2f"), Index > 0 ? TEXT(" / ") : TEXT(""), *Rates->ContainerPtrToValuePtr<float>(Params, Index));
			}
			Seen.FindOrAdd(Key) += 1;
		}
		FString Report;
		for (const TPair<FString, int32>& Pair : Seen)
		{
			Report += FString::Printf(TEXT(" | %s (%d)"), *Pair.Key, Pair.Value);
		}
		UE_LOG(LogGTPMassSettings, Log, TEXT("MassTraffic tick rates of %d vehicle simulation trait(s), High / Medium / Low / Off in s (0 = every frame)%s"),
			Traits.Num(), Report.IsEmpty() ? TEXT(" | none loaded") : *Report);
	}
}

class FGTPMassModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// MassTraffic's lane filters, pointed at our ZoneGraph tags once every plugin is loaded and
		// before any map: the game and the package need no config file for them. Nothing to do when
		// the optional Traffic plugin is absent (Mac, CI).
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			if (UClass* Settings = FindObject<UClass>(nullptr, TEXT("/Script/MassTraffic.MassTrafficSettings")))
			{
				UGTPZoneGraphImport::ConfigureTrafficLaneFilters(TEXT("Car"), TEXT("Intersection"));
				GTPMassModuleLocal::ApplyCitySampleDriving(*Settings);
				GTPMassModuleLocal::ApplyPhysicsRadius();
			}
		});
		// Once the map's actors are there (its spawners' entity configs loaded), before BeginPlay.
		FWorldDelegates::OnWorldInitializedActors.AddLambda([](const UWorld::FActorsInitializedParams& Params)
		{
			GTPMassModuleLocal::LogVehicleTickRates(Params.World);
		});
	}
};

IMPLEMENT_MODULE(FGTPMassModule, GTPMass);
