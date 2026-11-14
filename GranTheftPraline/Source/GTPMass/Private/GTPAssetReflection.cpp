#include "GTPAssetReflection.h"

#include "Dom/JsonObject.h"
#include "Misc/OutputDeviceNull.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace GTPAssetReflectionLocal
{
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
