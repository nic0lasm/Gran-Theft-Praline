// Just enough of HLSL in C++ to compile the body of the backdrop facade shader outside Unreal:
// the code of apply_city_materials.BACKDROP_HLSL is included as it is, so a typo or an
// undeclared name fails here, in CI, and not in the editor of the reference PC.
#pragma once

#include <cmath>

namespace hlsl
{
struct float2
{
	float x, y;
	float2() = default;
	float2(double a, double b) : x(float(a)), y(float(b)) {}
	float2& operator/=(float s) { x /= s; y /= s; return *this; }
};

struct float3
{
	union
	{
		struct { float x, y, z; };
		float2 xy;
	};
	float3() : x(0), y(0), z(0) {}
	float3(double a, double b, double c) : x(float(a)), y(float(b)), z(float(c)) {}
};

struct float4
{
	float x, y, z, w;
	float4(float3 v, double a) : x(v.x), y(v.y), z(v.z), w(float(a)) {}
};

inline float2 operator-(float2 a) { return float2(-a.x, -a.y); }
inline float2 operator*(float2 a, float s) { return float2(a.x * s, a.y * s); }
inline float3 operator+(float3 a, float3 b) { return float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline float3 operator-(float3 a, float3 b) { return float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline float3 operator*(float3 a, float3 b) { return float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline float3 operator*(float3 a, float s) { return float3(a.x * s, a.y * s, a.z * s); }
inline float3 operator*(float s, float3 a) { return a * s; }

inline float abs(double v) { return float(std::fabs(v)); }
inline float floor(double v) { return float(std::floor(v)); }
inline float sin(double v) { return float(std::sin(v)); }
inline float atan2(double a, double b) { return float(std::atan2(a, b)); }
inline float max(double a, double b) { return float(a > b ? a : b); }
inline float min(double a, double b) { return float(a < b ? a : b); }
inline float frac(double v) { return float(v - std::floor(v)); }
inline float step(double edge, double v) { return v >= edge ? 1.0f : 0.0f; }
inline float saturate(double v) { return float(v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float3 lerp(float3 a, float3 b, float t) { return a + (b - a) * t; }
inline float dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
inline float dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(float2 a) { return std::sqrt(dot(a, a)); }
inline float length(float3 a) { return std::sqrt(dot(a, a)); }
inline float3 normalize(float3 a) { return a * (1.0f / length(a)); }
}   // namespace hlsl
