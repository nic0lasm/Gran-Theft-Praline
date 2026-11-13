#include "GTPParkedCars.h"

#include "GTPLog.h"
#include "CollisionQueryParams.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPParkedCars, Log, All);

AGTPParkedCars::AGTPParkedCars()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

FString AGTPParkedCars::ParkingFileFor(const FString& MapName)
{
	return FPaths::ProjectContentDir() / TEXT("City/Traffic") / (MapName + TEXT(".parking.csv"));
}

int32 AGTPParkedCars::Build(const FString& File)
{
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *File) || Rows.Num() < 2)
	{
		GTP_TRACE(LogGTPParkedCars, TEXT("parked cars: no slots in %s"), *File);
		return 0;
	}
	for (const FSoftObjectPath& Path : Meshes)
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
		if (!Mesh)
		{
			GTP_TRACE(LogGTPParkedCars, TEXT("parked cars: mesh %s not found"), *Path.ToString());
			continue;
		}
		UHierarchicalInstancedStaticMeshComponent* Model = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Model->SetMobility(EComponentMobility::Movable);
		Model->SetStaticMesh(Mesh);
		Model->SetupAttachment(RootComponent);
		Model->SetNumCustomDataFloats(3);
		Model->SetCullDistances(CullStart, CullEnd);
		Model->SetCastShadow(bCastShadows);
		Model->SetCollisionProfileName(TEXT("BlockAll"));
		Model->RegisterComponent();
		Models.Add(Model);
	}
	if (Models.Num() == 0)
	{
		return 0;
	}
	if (Colours.Num() == 0)
	{
		Colours.Add(FLinearColor(0.8f, 0.8f, 0.8f));
	}
	int32 Placed = 0;
	int32 Missed = 0;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPParkedGround), false, this);
	const FCollisionObjectQueryParams Ground(ECC_WorldStatic);
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		TArray<FString> Fields;
		Rows[RowIndex].ParseIntoArray(Fields, TEXT(","), false);
		if (Fields.Num() < 3)
		{
			continue;
		}
		const double SlotX = FCString::Atod(*Fields[0]);
		const double SlotY = FCString::Atod(*Fields[1]);
		const float SlotYaw = FCString::Atof(*Fields[2]);
		// The street level is near Z = 0, like the lanes. Same trace as
		// AGTPTrafficNetwork::FindSpawnPoint (visibility channel, which every street
		// mesh blocks): the city v2 ground is not found as a WorldStatic object.
		FHitResult Hit;
		const FVector Top(SlotX, SlotY, 3000.0);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.0, 0.0, 6000.0), ECC_Visibility, Params)
			&& !GetWorld()->LineTraceSingleByObjectType(Hit, Top, Top - FVector(0.0, 0.0, 6000.0), Ground, Params))
		{
			if (Missed++ == 0)
			{
				GTP_TRACE(LogGTPParkedCars, TEXT("parked cars: no ground under slot %.0f %.0f"), SlotX, SlotY);
			}
			continue;
		}
		// Same model and colour at every run: picked from the slot position.
		const uint32 Seed = HashCombine(GetTypeHash(FMath::RoundToInt(SlotX)), GetTypeHash(FMath::RoundToInt(SlotY)));
		UHierarchicalInstancedStaticMeshComponent* Model = Models[Seed % static_cast<uint32>(Models.Num())];
		const FLinearColor& Paint = Colours[(Seed / 7u) % static_cast<uint32>(Colours.Num())];
		const int32 Instance = Model->AddInstance(FTransform(FRotator(0.f, SlotYaw, 0.f), Hit.ImpactPoint), true);
		Model->SetCustomDataValue(Instance, 0, Paint.R, false);
		Model->SetCustomDataValue(Instance, 1, Paint.G, false);
		Model->SetCustomDataValue(Instance, 2, Paint.B, false);
		++Placed;
	}
	for (UHierarchicalInstancedStaticMeshComponent* Model : Models)
	{
		Model->MarkRenderStateDirty();
	}
	GTP_TRACE(LogGTPParkedCars, TEXT("parked cars: %d placed, %d without ground, from %s (%d models)"), Placed, Missed, *File, Models.Num());
	return Placed;
}
