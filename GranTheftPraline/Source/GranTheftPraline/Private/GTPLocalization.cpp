#include "GTPLocalization.h"

namespace
{
	GTPUI::ELanguage CurrentLanguage = GTPUI::ELanguage::French;
}

GTPUI::ELanguage GTPGetLanguage()
{
	return CurrentLanguage;
}

void GTPSetLanguage(GTPUI::ELanguage Language)
{
	CurrentLanguage = Language;
}

FString GTPLocString(const char* Key)
{
	return FString(UTF8_TO_TCHAR(GTPUI::Find(Key, CurrentLanguage)));
}

FText GTPLoc(const char* Key)
{
	return FText::FromString(GTPLocString(Key));
}

FText GTPLocFormat(const char* Key, const FFormatOrderedArguments& Args)
{
	return FText::Format(FText::FromString(GTPLocString(Key)), Args);
}
