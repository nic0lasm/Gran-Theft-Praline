#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPCustomerQueue.h"
#include "GTPCustomers.generated.h"

class AGTPPedestrian;
class ULevel;

/**
 * Customers queuing at the tills of a restaurant interior (step 6 of
 * docs/gameplay/reference-interior.md): pedestrians lent by the pool
 * (UGTPPedestrianSubsystem::AcquireFromPool) stand on the spots tagged
 * GTP_CustomerSpot=q<line>_<place> by tools/interiors/unreal/build_interior_level.py, facing
 * the counter. GTPCustomerQueue.h decides who is served, who steps forward and who comes in;
 * during an armed threat in the shop the queue freezes. A served customer goes back to the
 * pool. Started and stopped with the interior by UGTPInteriorStreaming.
 *
 * Command line: -InteriorCustomers=N  customers at once (default 6 with -Interiors=1; 0: none).
 * Log lines "Customers: ..." on start, stop and every minute.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCustomerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void OpenShop(ULevel* InteriorLevel, const FString& InteriorId);
	void CloseShop();

	int32 GetCustomerCount() const { return Customers.Num(); }

private:
	void Apply(const GTPCustomers::FEvent& Event);
	bool IsFrozen() const;

	GTPCustomers::FRun Run;
	TArray<TArray<FTransform>> Spots;   // per line, front first
	TMap<int32, TWeakObjectPtr<AGTPPedestrian>> Customers;
	FString ShopId;
	size_t EventsSeen = 0;
	float Accumulator = 0.f;
	float SinceLog = 0.f;
	int32 Refused = 0;
	bool bOpen = false;
};
