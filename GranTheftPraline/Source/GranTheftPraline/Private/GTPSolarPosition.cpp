#include "GTPSolarPosition.h"

namespace
{
	constexpr double SolarDegToRad = UE_DOUBLE_PI / 180.0;

	double Wrap360(double Value)
	{
		Value = FMath::Fmod(Value, 360.0);
		return Value < 0.0 ? Value + 360.0 : Value;
	}

	/** Last Sunday of a month at 01:00 UTC, when the EU clocks change. */
	FDateTime LastSundayOneUtc(int32 Year, int32 Month)
	{
		FDateTime Day(Year, Month, FDateTime::DaysInMonth(Year, Month), 1);
		while (Day.GetDayOfWeek() != EDayOfWeek::Sunday)
		{
			Day -= FTimespan::FromDays(1.0);
		}
		return Day;
	}

	/** Altitude / azimuth of a body from its right ascension and declination (radians). */
	FGTPSkyPosition FromEquatorial(double LatDeg, double LocalSiderealDeg, double RightAscension, double Declination)
	{
		const double Phi = LatDeg * SolarDegToRad;
		const double HourAngle = LocalSiderealDeg * SolarDegToRad - RightAscension;
		const double SinAlt = FMath::Sin(Phi) * FMath::Sin(Declination) + FMath::Cos(Phi) * FMath::Cos(Declination) * FMath::Cos(HourAngle);
		FGTPSkyPosition Out;
		Out.ElevationDeg = FMath::Asin(FMath::Clamp(SinAlt, -1.0, 1.0)) / SolarDegToRad;
		Out.AzimuthDeg = Wrap360(FMath::Atan2(FMath::Sin(HourAngle),
			FMath::Cos(HourAngle) * FMath::Sin(Phi) - FMath::Tan(Declination) * FMath::Cos(Phi)) / SolarDegToRad + 180.0);
		return Out;
	}
}

FGTPSkyPosition GTPSolar::Sun(double LatDeg, double LonDeg, const FDateTime& Utc)
{
	const double HourUtc = Utc.GetHour() + Utc.GetMinute() / 60.0 + Utc.GetSecond() / 3600.0;
	const double DaysInYear = FDateTime::IsLeapYear(Utc.GetYear()) ? 366.0 : 365.0;
	const double Gamma = 2.0 * UE_DOUBLE_PI / DaysInYear * (Utc.GetDayOfYear() - 1 + (HourUtc - 12.0) / 24.0);

	const double EqTimeMinutes = 229.18 * (0.000075 + 0.001868 * FMath::Cos(Gamma) - 0.032077 * FMath::Sin(Gamma)
		- 0.014615 * FMath::Cos(2 * Gamma) - 0.040849 * FMath::Sin(2 * Gamma));
	const double Declination = 0.006918 - 0.399912 * FMath::Cos(Gamma) + 0.070257 * FMath::Sin(Gamma)
		- 0.006758 * FMath::Cos(2 * Gamma) + 0.000907 * FMath::Sin(2 * Gamma)
		- 0.002697 * FMath::Cos(3 * Gamma) + 0.00148 * FMath::Sin(3 * Gamma);

	const double TrueSolarMinutes = HourUtc * 60.0 + EqTimeMinutes + 4.0 * LonDeg;
	const double HourAngle = (TrueSolarMinutes / 4.0 - 180.0) * SolarDegToRad;
	const double Phi = LatDeg * SolarDegToRad;
	const double CosZenith = FMath::Sin(Phi) * FMath::Sin(Declination) + FMath::Cos(Phi) * FMath::Cos(Declination) * FMath::Cos(HourAngle);

	FGTPSkyPosition Out;
	Out.ElevationDeg = 90.0 - FMath::Acos(FMath::Clamp(CosZenith, -1.0, 1.0)) / SolarDegToRad;
	Out.AzimuthDeg = Wrap360(FMath::Atan2(FMath::Sin(HourAngle),
		FMath::Cos(HourAngle) * FMath::Sin(Phi) - FMath::Tan(Declination) * FMath::Cos(Phi)) / SolarDegToRad + 180.0);
	return Out;
}

FGTPSkyPosition GTPSolar::Moon(double LatDeg, double LonDeg, const FDateTime& Utc, double& OutIllumination)
{
	// Days since J2000.0 (2000-01-01 12:00 TT, UTC is close enough here).
	const double D = Utc.GetJulianDay() - 2451545.0;

	const double MeanLongitude = Wrap360(218.316 + 13.176396 * D);
	const double MeanAnomaly = Wrap360(134.963 + 13.064993 * D);
	const double ArgLatitude = Wrap360(93.272 + 13.229350 * D);
	const double Longitude = (MeanLongitude + 6.289 * FMath::Sin(MeanAnomaly * SolarDegToRad)) * SolarDegToRad;
	const double Latitude = 5.128 * FMath::Sin(ArgLatitude * SolarDegToRad) * SolarDegToRad;
	const double Obliquity = 23.4397 * SolarDegToRad;

	const double RightAscension = FMath::Atan2(FMath::Sin(Longitude) * FMath::Cos(Obliquity) - FMath::Tan(Latitude) * FMath::Sin(Obliquity),
		FMath::Cos(Longitude));
	const double Declination = FMath::Asin(FMath::Sin(Latitude) * FMath::Cos(Obliquity)
		+ FMath::Cos(Latitude) * FMath::Sin(Obliquity) * FMath::Sin(Longitude));
	const double LocalSidereal = Wrap360(280.16 + 360.9856235 * D + LonDeg);

	// Phase from the elongation to the sun's ecliptic longitude.
	const double SunMeanAnomaly = (357.528 + 0.9856003 * D) * SolarDegToRad;
	const double SunLongitude = (280.460 + 0.9856474 * D + 1.915 * FMath::Sin(SunMeanAnomaly) + 0.020 * FMath::Sin(2 * SunMeanAnomaly)) * SolarDegToRad;
	OutIllumination = 0.5 * (1.0 - FMath::Cos(Longitude - SunLongitude));

	return FromEquatorial(LatDeg, LocalSidereal, RightAscension, Declination);
}

int32 GTPSolar::ParisUtcOffsetHours(const FDateTime& Utc)
{
	const int32 Year = Utc.GetYear();
	return (Utc >= LastSundayOneUtc(Year, 3) && Utc < LastSundayOneUtc(Year, 10)) ? 2 : 1;
}

FDateTime GTPSolar::LocalToUtc(const FDateTime& Local)
{
	// Guess with winter time, then correct; the ambiguous hour in October resolves to summer time.
	const FDateTime Winter = Local - FTimespan::FromHours(1.0);
	const FDateTime Summer = Local - FTimespan::FromHours(2.0);
	return ParisUtcOffsetHours(Summer) == 2 ? Summer : Winter;
}

FRotator GTPSolar::LightRotation(const FGTPSkyPosition& Position)
{
	// Unreal yaw goes from +X (east) towards +Y (south), so compass azimuth A
	// points along yaw A - 90; the light travels the opposite way.
	return FRotator(-Position.ElevationDeg, Wrap360(Position.AzimuthDeg + 90.0), 0.0);
}
