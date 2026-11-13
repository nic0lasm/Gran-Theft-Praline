#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPZoneGraphImport.generated.h"

/**
 * Places the ZoneGraph of the city map from zonegraph.json (tools/city/zonegraph.py), so the
 * map is never edited by hand: run_city_import.sh calls it through
 * tools/city/unreal/place_zonegraph.py (ADR-0015).
 *
 * One ZoneShape actor per shape, tagged GTPZoneGraph (the previous ones are removed first):
 * driving lanes (one-way "Car" lane profile, tag Car), junction polygons (tags Car and
 * Intersection, one lane-profile point per lane end), walkways (two-way "Pedestrian" profile,
 * tag Pedestrian) and crossings (tags Pedestrian and Crossing). Points are dropped onto the
 * ground by a trace. Missing ZoneGraph tags and lane profiles are added to the settings.
 */
UCLASS()
class GTPMASS_API UGTPZoneGraphImport : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Builds the shapes in the world of WorldContext. Returns a JSON report: counts and errors. */
	UFUNCTION(BlueprintCallable, Category = "GTP|Mass", meta = (WorldContext = "WorldContext"))
	static FString BuildFromFile(UObject* WorldContext, const FString& JsonPath);

	/**
	 * Points MassTraffic's ZoneGraph lane filters at our tags (place_masstraffic.py): every
	 * FZoneGraphTagFilter of MassTrafficSettings whose name says Intersection gets IntersectionTag,
	 * crosswalk and pedestrian ones are left, every other one gets LaneTag. The tags must already
	 * be in the ZoneGraph settings (saved by BuildFromFile). The settings are written to their
	 * default config file. Returns a JSON report: each property and what was done.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Mass")
	static FString ConfigureTrafficLaneFilters(FName LaneTag, FName IntersectionTag);
};
