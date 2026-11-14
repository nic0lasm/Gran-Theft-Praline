#include "GTPMassSpawnerSetup.h"

#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawner.h"
#include "UObject/UnrealType.h"

FString UGTPMassSpawnerSetup::SetSpawnDataGenerator(AActor* Spawner, const FString& GeneratorClassPath)
{
	AMassSpawner* MassSpawner = Cast<AMassSpawner>(Spawner);
	if (!MassSpawner)
	{
		return TEXT("error: not a MassSpawner");
	}
	UClass* Class = LoadClass<UMassEntitySpawnDataGeneratorBase>(nullptr, *GeneratorClassPath);
	if (!Class)
	{
		return TEXT("error: no spawn data generator class ") + GeneratorClassPath;
	}
	// Protected on AMassSpawner: reached through reflection.
	const FArrayProperty* Property = CastField<FArrayProperty>(AMassSpawner::StaticClass()->FindPropertyByName(TEXT("SpawnDataGenerators")));
	const FStructProperty* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
	if (!Inner || Inner->Struct != FMassSpawnDataGenerator::StaticStruct())
	{
		return TEXT("error: AMassSpawner has no SpawnDataGenerators array of FMassSpawnDataGenerator");
	}
	MassSpawner->Modify();
	UMassEntitySpawnDataGeneratorBase* Generator = NewObject<UMassEntitySpawnDataGeneratorBase>(MassSpawner, Class, NAME_None, RF_Transactional);
	FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(MassSpawner));
	Helper.EmptyValues();
	const int32 Index = Helper.AddValue();
	FMassSpawnDataGenerator* Entry = reinterpret_cast<FMassSpawnDataGenerator*>(Helper.GetRawPtr(Index));
	Entry->GeneratorInstance = Generator;
	Entry->Proportion = 1.f;
	MassSpawner->MarkPackageDirty();
	return Generator->GetPathName();
}
