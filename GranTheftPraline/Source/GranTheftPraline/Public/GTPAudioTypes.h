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

/** Mixing category: one sound class each, children of Master (ADR-0009). */
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
	/** Zone layers of the ambience bed (pedestrian street, quay, bridge, night, crowd). */
	Zones,
	/** Rain on the street and on the car roof. */
	Weather,
	/** Police radio heard by the player while wanted (2D). */
	Radio,
	Count UMETA(Hidden)
};

/**
 * Interface sounds, folder /Game/Sound/SFX/UI/<Name>/ each
 * (UGTPAudioMix::PlayUI, menus and HUD of ADR-0007).
 */
UENUM(BlueprintType)
enum class EGTPUISound : uint8
{
	/** Focus moves to another button or tab. */
	Navigate,
	Confirm,
	Back,
	/** A setting changes value (slider step, on/off). */
	Toggle,
	/** A menu or the big map opens, or closes. */
	Open,
	Close,
	/** A wanted star is gained, or the last one is lost. */
	StarUp,
	StarDown,
	/** Zoom step of the map. */
	MapZoom,
	Count UMETA(Hidden)
};

/** Voice reactions of people in the street, folder /Game/Sound/SFX/Voices/<Name>/. */
UENUM(BlueprintType)
enum class EGTPVoiceKind : uint8
{
	/** Frightened: a car or a crash close by, running away. */
	Scream,
	/** Hit by a car or knocked over. */
	Hurt,
	/** Short surprise (near miss). */
	Gasp,
	Count UMETA(Hidden)
};

/** Police radio calls, folder /Game/Sound/SFX/Radio/<Name>/ (no words: tones and squelched chatter). */
UENUM(BlueprintType)
enum class EGTPRadioCall : uint8
{
	/** Background traffic of the channel while the player is wanted. */
	Chatter,
	/** Alert tones then a call: a star more. */
	Alert,
	/** End of a pursuit. */
	Clear,
	Count UMETA(Hidden)
};

/** Ambience zone of the zone grid (tools/audio/zone_map.py), same byte values. */
UENUM(BlueprintType)
enum class EGTPAmbienceZone : uint8
{
	Street,
	Pedestrian,
	Quay,
	Bridge,
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
