#include "GTPZoneGraphImport.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphTypes.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPZoneGraph, Log, All);

namespace GTPZoneGraphImportLocal
{
const FName ActorTag(TEXT("GTPZoneGraph"));

struct FReport
{
	TMap<FString, int32> Counts;
	TMap<FString, FString> Info;
	TArray<FString> Errors;

	void Error(const FString& Text)
	{
		UE_LOG(LogGTPZoneGraph, Warning, TEXT("zonegraph import: %s"), *Text);
		if (Errors.Num() < 50)
		{
			Errors.Add(Text);
		}
	}

	FString ToJson() const
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> CountObject = MakeShared<FJsonObject>();
		for (const TPair<FString, int32>& Pair : Counts)
		{
			CountObject->SetNumberField(Pair.Key, Pair.Value);
		}
		Root->SetObjectField(TEXT("counts"), CountObject);
		TSharedRef<FJsonObject> InfoObject = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : Info)
		{
			InfoObject->SetStringField(Pair.Key, Pair.Value);
		}
		Root->SetObjectField(TEXT("info"), InfoObject);
		TArray<TSharedPtr<FJsonValue>> ErrorValues;
		for (const FString& Text : Errors)
		{
			ErrorValues.Add(MakeShared<FJsonValueString>(Text));
		}
		Root->SetArrayField(TEXT("errors"), ErrorValues);
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
};

/** The tag called Name in the ZoneGraph settings, added in the first free slot when missing. */
bool FindOrAddTag(FName Name, FZoneGraphTag& OutTag, FReport& Report)
{
	UZoneGraphSettings* Settings = GetMutableDefault<UZoneGraphSettings>();
	const FStructProperty* Property = CastField<FStructProperty>(UZoneGraphSettings::StaticClass()->FindPropertyByName(TEXT("Tags")));
	if (!Settings || !Property || Property->Struct != FZoneGraphTagInfo::StaticStruct())
	{
		Report.Error(TEXT("ZoneGraphSettings has no Tags array of FZoneGraphTagInfo"));
		return false;
	}
	int32 Free = INDEX_NONE;
	for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
	{
		FZoneGraphTagInfo* Info = Property->ContainerPtrToValuePtr<FZoneGraphTagInfo>(Settings, Index);
		if (Info->Name == Name)
		{
			OutTag = Info->Tag;
			return true;
		}
		if (Free == INDEX_NONE && Info->Name.IsNone() && Index > 0)
		{
			Free = Index;
		}
	}
	if (Free == INDEX_NONE)
	{
		Report.Error(FString::Printf(TEXT("no free ZoneGraph tag slot for %s"), *Name.ToString()));
		return false;
	}
	FZoneGraphTagInfo* Info = Property->ContainerPtrToValuePtr<FZoneGraphTagInfo>(Settings, Free);
	Info->Name = Name;
	Info->Color = FColor::MakeRandomSeededColor(Free * 7919);
	Info->Tag = FZoneGraphTag(static_cast<uint8>(Free));
	OutTag = Info->Tag;
	Report.Counts.FindOrAdd(TEXT("tags_added"))++;
	return true;
}

/** The tag called Name in the ZoneGraph settings, never added (the import adds them). */
bool FindTag(FName Name, FZoneGraphTag& OutTag)
{
	UZoneGraphSettings* Settings = GetMutableDefault<UZoneGraphSettings>();
	const FStructProperty* Property = CastField<FStructProperty>(UZoneGraphSettings::StaticClass()->FindPropertyByName(TEXT("Tags")));
	if (!Settings || !Property || Property->Struct != FZoneGraphTagInfo::StaticStruct())
	{
		return false;
	}
	for (int32 Index = 1; Index < Property->ArrayDim; ++Index)
	{
		const FZoneGraphTagInfo* Info = Property->ContainerPtrToValuePtr<FZoneGraphTagInfo>(Settings, Index);
		if (Info->Name == Name)
		{
			OutTag = Info->Tag;
			return true;
		}
	}
	return false;
}

/**
 * Writes only the named properties of Settings to its default config file (editor only), not the
 * whole object: the file in Git then holds our values and nothing else. Returns the file and the
 * properties written.
 */
FString SaveProperties(UObject* Settings, const TArray<FName>& Names)
{
#if WITH_EDITOR
	const FString File = Settings->GetDefaultConfigFilename();
	TArray<FString> Saved;
	for (const FName& Name : Names)
	{
		const FProperty* Property = Settings->GetClass()->FindPropertyByName(Name);
		if (Property && Property->HasAnyPropertyFlags(CPF_Config))
		{
			Settings->UpdateSinglePropertyInConfigFile(Property, File);
			Saved.Add(Name.ToString());
		}
	}
	return FPaths::GetCleanFilename(File) + TEXT(": ") + (Saved.Num() ? FString::Join(Saved, TEXT(", ")) : FString(TEXT("nothing saved")));
#else
	(void)Settings;
	(void)Names;
	return TEXT("not saved: editor only");
#endif
}

FZoneGraphTagMask MaskOf(const TArray<TSharedPtr<FJsonValue>>& Names, FReport& Report)
{
	FZoneGraphTagMask Mask;
	for (const TSharedPtr<FJsonValue>& Value : Names)
	{
		FZoneGraphTag Tag;
		if (FindOrAddTag(FName(*Value->AsString()), Tag, Report))
		{
			Mask.Add(Tag);
		}
	}
	return Mask;
}

/** The lane profile called Name, added to the settings from its JSON description when missing. */
const FZoneLaneProfile* FindOrAddProfile(FName Name, const TSharedPtr<FJsonObject>& Description, FReport& Report)
{
	UZoneGraphSettings* Settings = GetMutableDefault<UZoneGraphSettings>();
	for (const FZoneLaneProfile& Profile : Settings->GetLaneProfiles())
	{
		if (Profile.Name == Name)
		{
			return &Profile;
		}
	}
	const FArrayProperty* Property = CastField<FArrayProperty>(UZoneGraphSettings::StaticClass()->FindPropertyByName(TEXT("LaneProfiles")));
	if (!Property || !Description.IsValid())
	{
		Report.Error(FString::Printf(TEXT("cannot add lane profile %s"), *Name.ToString()));
		return nullptr;
	}
	FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Settings));
	const int32 Index = Helper.AddValue();
	FZoneLaneProfile* Profile = reinterpret_cast<FZoneLaneProfile*>(Helper.GetRawPtr(Index));
	Profile->Name = Name;
	Profile->ID = FGuid::NewGuid();
	const TArray<TSharedPtr<FJsonValue>>* Lanes = nullptr;
	if (Description->TryGetArrayField(TEXT("lanes"), Lanes))
	{
		for (const TSharedPtr<FJsonValue>& LaneValue : *Lanes)
		{
			const TSharedPtr<FJsonObject> Lane = LaneValue->AsObject();
			FZoneLaneDesc Desc;
			Desc.Width = static_cast<float>(Lane->GetNumberField(TEXT("width_cm")));
			Desc.Direction = Lane->GetStringField(TEXT("direction")) == TEXT("backward") ? EZoneLaneDirection::Backward : EZoneLaneDirection::Forward;
			const TArray<TSharedPtr<FJsonValue>>* TagNames = nullptr;
			if (Lane->TryGetArrayField(TEXT("tags"), TagNames))
			{
				Desc.Tags = MaskOf(*TagNames, Report);
			}
			Profile->Lanes.Add(Desc);
		}
	}
	Report.Counts.FindOrAdd(TEXT("profiles_added"))++;
	return Profile;
}

FVector OnGround(UWorld* World, double X, double Y)
{
	const FVector Top(X, Y, 100000.0);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPZoneGraphGround), true);
	if (World->LineTraceSingleByChannel(Hit, Top, FVector(X, Y, -100000.0), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint + FVector(0.0, 0.0, 10.0);
	}
	return FVector(X, Y, 0.0);
}

void SetShapeType(UZoneShapeComponent* Shape, FZoneShapeType Type, FReport& Report)
{
	if (FProperty* Property = UZoneShapeComponent::StaticClass()->FindPropertyByName(TEXT("ShapeType")))
	{
		*Property->ContainerPtrToValuePtr<FZoneShapeType>(Shape) = Type;
		return;
	}
	Report.Error(TEXT("ZoneShapeComponent has no ShapeType"));
}

void SetShapeTags(UZoneShapeComponent* Shape, const FZoneGraphTagMask& Mask, FReport& Report)
{
	const FStructProperty* Property = CastField<FStructProperty>(UZoneShapeComponent::StaticClass()->FindPropertyByName(TEXT("Tags")));
	if (Property && Property->Struct == FZoneGraphTagMask::StaticStruct())
	{
		*Property->ContainerPtrToValuePtr<FZoneGraphTagMask>(Shape) = Mask;
		return;
	}
	Report.Error(TEXT("ZoneShapeComponent has no Tags mask"));
}
} // namespace GTPZoneGraphImportLocal

FString UGTPZoneGraphImport::BuildFromFile(UObject* WorldContext, const FString& JsonPath)
{
	using namespace GTPZoneGraphImportLocal;
	FReport Report;
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!World)
	{
		Report.Error(TEXT("no world"));
		return Report.ToJson();
	}
	if (!FFileHelper::LoadFileToString(Text, *JsonPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Report.Error(FString::Printf(TEXT("cannot read %s"), *JsonPath));
		return Report.ToJson();
	}

	// Idempotent: the shapes of the previous import go first.
	TArray<AActor*> Old;
	for (TActorIterator<AZoneShape> It(World); It; ++It)
	{
		if (It->ActorHasTag(ActorTag))
		{
			Old.Add(*It);
		}
	}
	for (AActor* Actor : Old)
	{
		World->DestroyActor(Actor);
	}
	Report.Counts.Add(TEXT("removed"), Old.Num());

	const TSharedPtr<FJsonObject>* Profiles = nullptr;
	Root->TryGetObjectField(TEXT("profiles"), Profiles);
	const TArray<TSharedPtr<FJsonValue>>* Shapes = nullptr;
	if (!Root->TryGetArrayField(TEXT("shapes"), Shapes))
	{
		Report.Error(TEXT("no shapes"));
		return Report.ToJson();
	}
	for (const TSharedPtr<FJsonValue>& ShapeValue : *Shapes)
	{
		const TSharedPtr<FJsonObject> Desc = ShapeValue->AsObject();
		const FString Name = Desc->GetStringField(TEXT("name"));
		const FString ProfileName = Desc->GetStringField(TEXT("profile"));
		const bool bPolygon = Desc->GetStringField(TEXT("type")) == TEXT("polygon");
		const TSharedPtr<FJsonObject> ProfileDesc = Profiles && (*Profiles)->HasField(ProfileName) ? (*Profiles)->GetObjectField(ProfileName) : nullptr;
		const FZoneLaneProfile* Profile = FindOrAddProfile(FName(*ProfileName), ProfileDesc, Report);
		const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (!Profile || !Desc->TryGetArrayField(TEXT("points"), Points) || Points->Num() < 2)
		{
			Report.Counts.FindOrAdd(TEXT("skipped"))++;
			continue;
		}

		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World->PersistentLevel, AZoneShape::StaticClass(), FName(*(TEXT("ZG_") + Name)));
		AZoneShape* Actor = World->SpawnActor<AZoneShape>(AZoneShape::StaticClass(), FTransform::Identity, Params);
		UZoneShapeComponent* Shape = Actor ? Actor->FindComponentByClass<UZoneShapeComponent>() : nullptr;
		if (!Shape)
		{
			Report.Error(FString::Printf(TEXT("%s: no ZoneShape component"), *Name));
			continue;
		}
		Actor->Tags.Add(ActorTag);
#if WITH_EDITOR
		Actor->SetFolderPath(TEXT("City/ZoneGraph"));
		Actor->SetActorLabel(TEXT("ZG_") + Name);
#endif
		SetShapeType(Shape, bPolygon ? FZoneShapeType::Polygon : FZoneShapeType::Spline, Report);
		Shape->SetCommonLaneProfile(FZoneLaneProfileRef(*Profile));
		const TArray<TSharedPtr<FJsonValue>>* TagNames = nullptr;
		if (Desc->TryGetArrayField(TEXT("tags"), TagNames))
		{
			SetShapeTags(Shape, MaskOf(*TagNames, Report), Report);
		}

		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Yaws = nullptr;
		Desc->TryGetArrayField(TEXT("roles"), Roles);
		Desc->TryGetArrayField(TEXT("yaw_deg"), Yaws);
		TArray<FZoneShapePoint>& ShapePoints = Shape->GetMutablePoints();
		ShapePoints.Reset();
		for (int32 Index = 0; Index < Points->Num(); ++Index)
		{
			const TArray<TSharedPtr<FJsonValue>>& XY = (*Points)[Index]->AsArray();
			FZoneShapePoint Point(OnGround(World, XY[0]->AsNumber(), XY[1]->AsNumber()));
			if (bPolygon)
			{
				// A lane end on the junction's edge: its lane profile faces into the polygon.
				// A lane leaving the junction runs the other way: the profile is reversed.
				const bool bOut = Roles && Roles->IsValidIndex(Index) && (*Roles)[Index]->AsString() == TEXT("out");
				const double Yaw = Yaws && Yaws->IsValidIndex(Index) ? (*Yaws)[Index]->AsNumber() : 0.0;
				Point.Type = FZoneShapePointType::LaneProfile;
				Point.Rotation = FRotator(0.0, bOut ? Yaw + 180.0 : Yaw, 0.0);
				Point.bReverseLaneProfile = bOut;
			}
			else
			{
				Point.Type = FZoneShapePointType::AutoBezier;
			}
			ShapePoints.Add(Point);
		}
		Shape->UpdateShape();
		Report.Counts.FindOrAdd(bPolygon ? TEXT("polygons") : (ProfileName == TEXT("Car") ? TEXT("car_splines") : TEXT("walk_splines")))++;
	}
	// The tags and lane profiles added above go to the settings' default config file: the next
	// editor run (place_masstraffic.py) and the game find them at the same bits as the shapes.
	Report.Info.Add(TEXT("zonegraph_settings_saved"), SaveProperties(GetMutableDefault<UZoneGraphSettings>(), {FName(TEXT("Tags")), FName(TEXT("LaneProfiles"))}));
	UE_LOG(LogGTPZoneGraph, Log, TEXT("zonegraph import: %s"), *Report.ToJson());
	return Report.ToJson();
}

FString UGTPZoneGraphImport::ConfigureTrafficLaneFilters(FName LaneTag, FName IntersectionTag)
{
	using namespace GTPZoneGraphImportLocal;
	FReport Report;
	FZoneGraphTag Lane;
	FZoneGraphTag Junction;
	if (!FindTag(LaneTag, Lane) || !FindTag(IntersectionTag, Junction))
	{
		Report.Error(FString::Printf(TEXT("ZoneGraph tag %s or %s not in the settings: run place_zonegraph.py first"),
			*LaneTag.ToString(), *IntersectionTag.ToString()));
		return Report.ToJson();
	}
	// Found by name, not linked: MassTraffic is an optional plugin, copied out of Git.
	UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/MassTraffic.MassTrafficSettings"));
	UObject* Settings = Class ? Class->GetDefaultObject() : nullptr;
	if (!Settings)
	{
		Report.Error(TEXT("MassTrafficSettings not found: plugin Traffic not loaded"));
		return Report.ToJson();
	}
	TArray<FName> Changed;
	for (TFieldIterator<FStructProperty> It(Class); It; ++It)
	{
		const FStructProperty* Property = *It;
		const FString Name = Property->GetName();
		if (Property->Struct != FZoneGraphTagFilter::StaticStruct())
		{
			if (Property->Struct->GetName().StartsWith(TEXT("ZoneGraphTag")))
			{
				Report.Info.Add(Name, TEXT("left as is: ") + Property->Struct->GetName());
			}
			continue;
		}
		if (Name.Contains(TEXT("Crosswalk")) || Name.Contains(TEXT("Pedestrian")))
		{
			Report.Info.Add(Name, TEXT("left as is"));
			continue;
		}
		const bool bIntersection = Name.Contains(TEXT("Intersection"));
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FZoneGraphTagFilter* Filter = Property->ContainerPtrToValuePtr<FZoneGraphTagFilter>(Settings, Index);
			FZoneGraphTagMask Mask;
			Mask.Add(bIntersection ? Junction : Lane);
			Filter->AnyTags = Mask;
			Filter->AllTags = FZoneGraphTagMask();
			Filter->NotTags = FZoneGraphTagMask();
		}
		Report.Info.Add(Name, (bIntersection ? IntersectionTag : LaneTag).ToString());
		Report.Counts.FindOrAdd(TEXT("filters_set"))++;
		Changed.Add(Property->GetFName());
	}
	Report.Info.Add(TEXT("masstraffic_settings_saved"), SaveProperties(Settings, Changed));
	UE_LOG(LogGTPZoneGraph, Log, TEXT("traffic lane filters: %s"), *Report.ToJson());
	return Report.ToJson();
}
