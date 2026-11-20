#include "Modules/ModuleManager.h"

#include "GTPZoneGraphImport.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"
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
			}
		});
	}
};

IMPLEMENT_MODULE(FGTPMassModule, GTPMass);
