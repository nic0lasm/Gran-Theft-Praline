#pragma once

#include "CoreMinimal.h"
#include "GTPAudioTypes.generated.h"

/**
 * Ground under a foot or a tyre. The order matches the byte values of the
 * surface map written by tools/audio/surface_map.py (GTPSurfaceLogic.h).
 */
UENUM(BlueprintType)
enum class EGTPSurface : uint8
{
	/** Carriageway. */
	Asphalt,
	/** Limestone or granite slabs: sidewalks, pedestrian streets, kerbs. */
	Stone,
	/** Old cobbles (pavés). */
	Cobble,
	/** Gravel and red sand (place Bellecour). */
	Gravel,
	Grass,
	/** Manhole covers, gratings, bridge joints, steel steps. */
	Metal,
	/** Decks, benches, wooden floors. */
	Wood,
	/** Puddles and wet ground (rain, 0.6). */
	Water,
	Count UMETA(Hidden)
};

/** Mixing category: one sound class each, children of Master (ADR-0006). */
UENUM(BlueprintType)
enum class EGTPAudioCategory : uint8
{
	Master,
	/** Parent of every world sound effect below. */
	Effects,
	Footsteps,
	Vehicles,
	Impacts,
	Sirens,
	Ambience,
	Music,
	Voice,
	Interface,
	Count UMETA(Hidden)
};

/** What a vehicle body hit. */
UENUM(BlueprintType)
enum class EGTPImpactKind : uint8
{
	/** Another car: sheet metal against sheet metal. */
	Car,
	/** A pole, a railing, a bollard. */
	Metal,
	/** A wall, a kerb, concrete or stone. */
	Wall,
	/** A person. */
	Body,
	/** Benches, bins, terrace furniture. */
	Wood,
	Count UMETA(Hidden)
};

/** Siren pattern of a French emergency vehicle. */
UENUM(BlueprintType)
enum class EGTPSirenMode : uint8
{
	Off,
	/** Police « deux tons »: la 435 Hz / ré 580 Hz, alternating. */
	TwoTone,
	/** Fast rising and falling wail, used for short bursts in a chase. */
	Yelp,
	Count UMETA(Hidden)
};
