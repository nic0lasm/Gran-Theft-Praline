// The paint of the cars of the traffic, written without engine types so tools/vehicles/paint_test checks it.
//
// Gate 104's film: the cars of the traffic are all of the blue of the car the player stole (a blue queue at 62 s, a
// blue jam from 128 to 158 s). Read in the editor on 26/11: seven of the nine City Sample cars have no colour of
// their own. Their paint (slot veh_carPaint, M_Veh_CarPaint) takes its colour from a datum City Sample's own
// traffic gives each car ("MassTraffic PackedParam1", "Paint Var Lookup"), which our cars do not get: every one of
// them reads 0 there, the same paint. The only car with a colour of its own, vehicle 12, sets the vector BaseColor
// with an alpha of 1, where the paint all the others inherit has an alpha of 0: BaseColor with alpha 1 is taken
// here as "this colour, not the looked-up one" (not proven until a gate shows it).
//
// The colours are those of a French street, in the shares cars are sold in (greys, whites and blacks seven in
// ten), linear values as the material wants them. A draw never gives the colour of the car made just before.
#pragma once

namespace GTPCarPaint
{
	struct FPaint
	{
		const char* Name;
		float R, G, B;
		float Share; // of a hundred cars
	};

	inline constexpr int NumPaints = 13;

	inline constexpr FPaint Paints[NumPaints] = {
		{"grey", 0.18f, 0.19f, 0.20f, 17.f},
		{"white", 0.80f, 0.80f, 0.78f, 20.f},
		{"black", 0.012f, 0.012f, 0.014f, 14.f},
		{"anthracite", 0.055f, 0.06f, 0.065f, 10.f},
		{"silver", 0.45f, 0.46f, 0.47f, 9.f},
		{"dark blue", 0.02f, 0.05f, 0.18f, 7.f},
		{"red", 0.42f, 0.02f, 0.02f, 6.f},
		{"grey blue", 0.17f, 0.26f, 0.36f, 4.f},
		{"burgundy", 0.15f, 0.012f, 0.02f, 3.f},
		{"dark green", 0.03f, 0.09f, 0.055f, 3.f},
		{"beige", 0.48f, 0.41f, 0.30f, 3.f},
		{"orange brown", 0.35f, 0.2042f, 0.f, 2.f},
		{"yellow", 0.72f, 0.52f, 0.03f, 2.f},
	};

	inline float TotalShare()
	{
		float Sum = 0.f;
		for (const FPaint& Each : Paints)
		{
			Sum += Each.Share;
		}
		return Sum;
	}

	/**
	 * The paint of the next car: U01 a number drawn from 0 to 1, Last the paint of the car made just before
	 * (-1: none). The shares are those of Paints with Last left out, so no two cars in a row are alike.
	 */
	inline int Draw(float U01, int Last = -1)
	{
		const float Left = TotalShare() - (Last >= 0 && Last < NumPaints ? Paints[Last].Share : 0.f);
		float At = (U01 < 0.f ? 0.f : (U01 > 1.f ? 1.f : U01)) * Left;
		int Picked = -1;
		for (int Index = 0; Index < NumPaints; ++Index)
		{
			if (Index == Last)
			{
				continue;
			}
			Picked = Index;
			if (At < Paints[Index].Share)
			{
				break;
			}
			At -= Paints[Index].Share;
		}
		return Picked;
	}
}
