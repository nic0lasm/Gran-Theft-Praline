#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace GTPOnFoot
{
/**
 * Somebody on foot does not shove what he walks into. A character's movement component pushes every simulating body
 * it runs into (bEnablePhysicsInteraction, on by default; the default push force, 750 000, is not scaled to what the
 * body weighs: on a car of 1.5 t that would be 5 m/s2 for each blocked move of each person; read in Epic's reference
 * and forum, not measured here). Gate 99's long take, 97 to 107 s: the stolen car stands empty on place Bellecour, two
 * officers run to it and along it, it goes sideways at 35 km/h, onto its roof and over the square, and no line of the
 * log says why. Call it wherever a character starts to walk; -PeopleShoveCars gives the push back.
 */
inline void NoShoving(ACharacter* Character)
{
	static const bool bShoves = FParse::Param(FCommandLine::Get(), TEXT("PeopleShoveCars"));
	if (UCharacterMovementComponent* Legs = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Legs->bEnablePhysicsInteraction = bShoves;
	}
}
} // namespace GTPOnFoot
