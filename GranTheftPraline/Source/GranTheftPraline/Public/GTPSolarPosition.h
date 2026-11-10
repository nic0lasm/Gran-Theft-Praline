#pragma once

#include "CoreMinimal.h"

/** Sun or moon position in the local sky. */
struct FGTPSkyPosition
{
	/** Degrees above the horizon (negative below). */
	double ElevationDeg = 0.0;
	/** Compass azimuth in degrees, from north, clockwise. */
	double AzimuthDeg = 0.0;
};

/**
 * Low-cost astronomy for the day-night cycle (docs/rendering/day-night-weather.md).
 * Sun: NOAA solar position equations (about 0.1 degree). Moon: truncated
 * series (about 1 degree), plenty for lighting.
 */
namespace GTPSolar
{
	/** Sun position for an observer at LatDeg / LonDeg (east positive) at a UTC time. */
	GRANTHEFTPRALINE_API FGTPSkyPosition Sun(double LatDeg, double LonDeg, const FDateTime& Utc);

	/** Moon position; OutIllumination is the lit fraction of the disc (0 new, 1 full). */
	GRANTHEFTPRALINE_API FGTPSkyPosition Moon(double LatDeg, double LonDeg, const FDateTime& Utc, double& OutIllumination);

	/** Offset of Lyon (Europe/Paris) local time to UTC in hours: +1 in winter, +2 in summer (EU rule). */
	GRANTHEFTPRALINE_API int32 ParisUtcOffsetHours(const FDateTime& Utc);

	/** UTC time of a Lyon local wall-clock time. */
	GRANTHEFTPRALINE_API FDateTime LocalToUtc(const FDateTime& Local);

	/**
	 * Rotation of a directional light shining from a sky position, in the
	 * city frame of tools/city (+X east, +Y south, +Z up). A light shines
	 * along its +X axis, away from the body.
	 */
	GRANTHEFTPRALINE_API FRotator LightRotation(const FGTPSkyPosition& Position);
}
