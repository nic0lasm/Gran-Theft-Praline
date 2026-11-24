// The squares' signed distance (tools/city/squares.py, hlsl()) outside Unreal: the text of the
// material's Custom node is included as it is and asked for the points read on stdin
// ("x y" a line), one answer a line. The tests compare it with the Python reference.
#include <cstdio>

#include "hlsl_shim.h"

namespace hlsl
{
static float SquareDistance(float2 XY)
{
#include "square_body.inc"
}
}   // namespace hlsl

int main()
{
	double X = 0.0, Y = 0.0;
	while (std::scanf("%lf %lf", &X, &Y) == 2)
	{
		std::printf("%.3f\n", double(hlsl::SquareDistance(hlsl::float2(X, Y))));
	}
	return 0;
}
