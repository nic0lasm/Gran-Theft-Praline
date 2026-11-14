#include "GTPAssetReflection.h"

#include "Dom/JsonObject.h"
#include "Misc/OutputDeviceNull.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

namespace GTPAssetReflectionLocal
{
/** Lists the references of one object (as it serializes them) into the watched packages. */
class FReferenceLister : public FArchiveUObject
{
public:
	FReferenceLister(UObject* InRoot, const TSet<FName>& InWatched, TArray<FString>& InOut)
		: Root(InRoot), Watched(InWatched), Out(InOut)
	{
		ArIsObjectReferenceCollector = true;
		ArIgnoreOuterRef = true;
		Root->Serialize(*this);
	}

	virtual FArchive& operator<<(UObject*& Object) override
	{
		if (Object && Object != Root && Watched.Contains(Object->GetPackage()->GetFName()))
		{
			Out.AddUnique(Root->GetPathName() + TEXT(" -> ") + Object->GetPathName());
		}
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPath& Value) override
	{
		if (Watched.Contains(Value.GetLongPackageFName()))
		{
			Out.AddUnique(Root->GetPathName() + TEXT(" ~> ") + Value.ToString());
		}
		return *this;
	}

	virtual FArchive& operator<<(FSoftObjectPtr& Value) override
	{
		FSoftObjectPath Path = Value.ToSoftObjectPath();
		return *this << Path;
	}

private:
	UObject* Root;
	const TSet<FName>& Watched;
	TArray<FString>& Out;
};

TSharedRef<FJsonObject> Describe(UObject* Object, int32 Depth)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("path"), Object->GetPathName());
	Out->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
	TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		FString Value;
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FString Item;
			Property->ExportText_InContainer(Index, Item, Object, nullptr, Object, PPF_None);
			Value += (Index > 0 ? TEXT(" | ") : TEXT("")) + Item;
		}
		if (Value.Len() > 16000)
		{
			Value = Value.Left(16000) + TEXT("...");
		}
		Properties->SetStringField(Property->GetName(), Value);
	}
	Out->SetObjectField(TEXT("properties"), Properties);
	if (Depth > 0)
	{
		TArray<UObject*> Children;
		GetObjectsWithOuter(Object, Children, /*bIncludeNestedObjects*/ false);
		TArray<TSharedPtr<FJsonValue>> Values;
		for (UObject* Child : Children)
		{
			if (Child && !Child->IsA<UPackage>() && !Child->HasAnyFlags(RF_Transient))
			{
				Values.Add(MakeShared<FJsonValueObject>(Describe(Child, Depth - 1)));
			}
		}
		Out->SetArrayField(TEXT("subobjects"), Values);
	}
	return Out;
}
} // namespace GTPAssetReflectionLocal

FString UGTPAssetReflection::DescribeObject(UObject* Object, int32 MaxDepth)
{
	if (!Object)
	{
		return TEXT("{}");
	}
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(GTPAssetReflectionLocal::Describe(Object, FMath::Clamp(MaxDepth, 0, 6)), Writer);
	return Out;
}

FString UGTPAssetReflection::SetPropertyText(const FString& ObjectPath, const FString& PropertyPath, const FString& Text)
{
	UObject* Object = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (!Object)
	{
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	}
	if (!Object)
	{
		return TEXT("no object ") + ObjectPath;
	}
	TArray<FString> Names;
	PropertyPath.ParseIntoArray(Names, TEXT("."));
	if (Names.Num() == 0)
	{
		return TEXT("empty property path");
	}
	const UStruct* Owner = Object->GetClass();
	void* Container = Object;
	FProperty* Top = nullptr;
	FProperty* Property = nullptr;
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		Property = Owner->FindPropertyByName(FName(*Names[Index]));
		if (!Property)
		{
			return FString::Printf(TEXT("%s has no property %s"), *Owner->GetName(), *Names[Index]);
		}
		Top = Top ? Top : Property;
		if (Index + 1 < Names.Num())
		{
			const FStructProperty* Struct = CastField<FStructProperty>(Property);
			if (!Struct)
			{
				return Names[Index] + TEXT(" is not a struct");
			}
			Container = Struct->ContainerPtrToValuePtr<void>(Container);
			Owner = Struct->Struct;
		}
	}
#if WITH_EDITOR
	Object->PreEditChange(Top);
#endif
	FOutputDeviceNull Errors;
	const TCHAR* End = Property->ImportText_InContainer(*Text, Container, Object, PPF_None, &Errors);
#if WITH_EDITOR
	FPropertyChangedEvent Event(Top, EPropertyChangeType::ValueSet);
	Object->PostEditChangeProperty(Event);
#endif
	if (!End)
	{
		return FString::Printf(TEXT("%s: cannot read \"%s\""), *PropertyPath, *Text.Left(200));
	}
	Object->MarkPackageDirty();
	return FString();
}

FString UGTPAssetReflection::ReplacePackageReferences(const FString& PackagePath, const TMap<FString, FString>& FromTo)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Errors;
	int32 Replaced = 0;
#if WITH_EDITOR
	UPackage* Package = LoadPackage(nullptr, *PackagePath, LOAD_None);
	if (!Package)
	{
		Errors.Add(MakeShared<FJsonValueString>(TEXT("cannot load ") + PackagePath));
	}
	else
	{
		// Each object of a source package, matched by its path inside the package with the object
		// of our copy (a duplicate: same names, the Blueprint's class and default object included).
		TMap<UObject*, UObject*> Replacements;
		TSharedRef<FJsonObject> Mapped = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : FromTo)
		{
			// Our copy's own source is mapped too: a reference left to it goes to the copy itself.
			if (Pair.Key == PackagePath)
			{
				continue;
			}
			UPackage* From = LoadPackage(nullptr, *Pair.Key, LOAD_None);
			UPackage* To = LoadPackage(nullptr, *Pair.Value, LOAD_None);
			if (!From || !To)
			{
				Errors.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("cannot load %s or %s"), *Pair.Key, *Pair.Value)));
				continue;
			}
			TArray<UObject*> FromObjects;
			GetObjectsWithPackage(From, FromObjects, /*bIncludeNestedObjects*/ true);
			int32 Count = 0;
			for (UObject* Object : FromObjects)
			{
				const FString Inside = Object->GetPathName(From);
				if (UObject* Match = StaticFindObject(UObject::StaticClass(), nullptr, *(To->GetPathName() + TEXT(".") + Inside)))
				{
					Replacements.Add(Object, Match);
					++Count;
				}
			}
			Mapped->SetNumberField(Pair.Key, Count);
		}
		Out->SetObjectField(TEXT("mapped"), Mapped);
		TArray<UObject*> Objects;
		GetObjectsWithPackage(Package, Objects, /*bIncludeNestedObjects*/ true);
		UBlueprint* Blueprint = nullptr;
		for (UObject* Object : Objects)
		{
			if (!Blueprint && Object->GetOuter() == Package)
			{
				Blueprint = Cast<UBlueprint>(Object);
			}
			if (Replacements.Num() > 0)
			{
				FArchiveReplaceObjectRef<UObject> Archive(Object, Replacements,
					EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
				Replaced += static_cast<int32>(Archive.GetCount());
			}
		}
		if (Replaced > 0)
		{
			Package->MarkPackageDirty();
			if (Blueprint)
			{
				FKismetEditorUtilities::CompileBlueprint(Blueprint);
				Out->SetStringField(TEXT("compiled"), Blueprint->Status == BS_UpToDate ? TEXT("up to date")
					: Blueprint->Status == BS_UpToDateWithWarnings ? TEXT("up to date with warnings")
					: Blueprint->Status == BS_Error ? TEXT("error") : TEXT("not compiled"));
				if (Blueprint->Status == BS_Error)
				{
					Errors.Add(MakeShared<FJsonValueString>(PackagePath + TEXT(": the Blueprint does not compile after the replacement")));
				}
			}
		}
	}
#else
	Errors.Add(MakeShared<FJsonValueString>(TEXT("editor only")));
#endif
	Out->SetNumberField(TEXT("replaced"), Replaced);
	Out->SetArrayField(TEXT("errors"), Errors);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Out, Writer);
	return Text;
}

FString UGTPAssetReflection::ListReferencesTo(const FString& PackagePath, const TArray<FString>& Watched)
{
	TArray<FString> Lines;
	UPackage* Package = LoadPackage(nullptr, *PackagePath, LOAD_None);
	if (!Package)
	{
		Lines.Add(TEXT("cannot load ") + PackagePath);
	}
	else
	{
		TSet<FName> WatchedNames;
		for (const FString& Name : Watched)
		{
			WatchedNames.Add(FName(*Name));
		}
		TArray<UObject*> Objects;
		GetObjectsWithPackage(Package, Objects, /*bIncludeNestedObjects*/ true);
		for (UObject* Object : Objects)
		{
			GTPAssetReflectionLocal::FReferenceLister Lister(Object, WatchedNames, Lines);
		}
	}
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Line : Lines)
	{
		Values.Add(MakeShared<FJsonValueString>(Line));
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Values, Writer);
	return Text;
}

int32 UGTPAssetReflection::RescanPackages(const TArray<FString>& PackagePaths)
{
	TArray<FString> Files;
	for (const FString& Path : PackagePaths)
	{
		FString File;
		if (FPackageName::TryConvertLongPackageNameToFilename(Path, File, FPackageName::GetAssetPackageExtension()))
		{
			Files.Add(FPaths::ConvertRelativePathToFull(File));
		}
	}
	if (Files.Num() > 0)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanFilesSynchronous(Files, /*bForceRescan*/ true);
	}
	return Files.Num();
}
