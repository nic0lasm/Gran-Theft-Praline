#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;

/**
 * Money on the HUD (economy API: GTPWallet.h): the balance in euros at the top right,
 * under the police stars, and a "+150" / "-20" flash after each change. Reads
 * UGTPWalletSubsystem::GetMoney each frame (a field read), holds no economy logic.
 * Compiles to nothing when GTPWallet.h is absent.
 */
namespace GTPHudMoney
{
	void Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale);
}
