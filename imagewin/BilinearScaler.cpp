/*
Copyright (C) 2005 The Pentagram Team
Copyright (C) 2010-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "pent_include.h"

#include "BilinearScaler.h"

#include "BilinearScalerInternal.h"
#include "manip.h"

namespace Pentagram {

	template <class uintX, class Manip, class uintS>
	class BilinearScalerInternal {
	public:
		static bool ScaleBilinear(
				SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy, uint_fast32_t sw, uint_fast32_t sh,
				uint8* pixel, uint_fast32_t dw, uint_fast32_t dh, uint_fast32_t pitch,
				bool clamp_src) {

			//
			// Clip the source rect to the size of tex and adjust dest rect as appropriate
			//
			uint_fast32_t tex_w = tex->w, tex_h = tex->h;

			// clip y
			if ((sh + sy) > tex_h) {
				auto nsh = tex_h - sy;
				dh       = (dh * nsh) / sh;
				sh       = nsh;
			}
			// clip x
			if ((sw + sx) > tex_w) {
				auto nsw = tex_w - sx;
				dw = (dh * nsw) / sw;
				sw = nsw;
			}

			//
			// Call the correct specialized function as appropriate
			//

			// 2x Scaling
			if ((sw * 2 == dw) && (sh * 2 == dh) && !(sh % 4) && !(sw % 4)) {
				return BilinearScalerInternal_2x<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// 2 X 2.4 Y
			else if (
					(sw * 2 == dw) && (dh * 5 == sh * 12) && !(sh % 5)
					&& !(sw % 4)) {
				return BilinearScalerInternal_X2Y24<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// 1 X 1.2 Y
			else if (
					(sw == dw) && (dh * 5 == sh * 6) && !(sh % 5)
					&& !(sw % 4)) {
				return BilinearScalerInternal_X1Y12<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// Arbitrary has no restrictions
			else {
				return BilinearScalerInternal_Arb<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			} 
		}
	};

	BilinearScaler::BilinearScaler() {
		Scale8To8  = nullptr;
		Scale8To32 = BilinearScalerInternal<
				uint_fast32_t, Manip8to32, uint8>::ScaleBilinear;
		Scale32To32 = BilinearScalerInternal<
				uint_fast32_t, Manip32to32, uint_fast32_t>::ScaleBilinear;

#ifdef COMPILE_ALL_BILINEAR_SCALERS
		Scale8To565 = BilinearScalerInternal<
				uint16, Manip8to565, uint8>::ScaleBilinear;
		Scale8To16 = BilinearScalerInternal<
				uint16, Manip8to16, uint8>::ScaleBilinear;
		Scale8To555 = BilinearScalerInternal<
				uint16, Manip8to555, uint8>::ScaleBilinear;

		Scale16To16 = BilinearScalerInternal<
				uint16, Manip16to16, uint16>::ScaleBilinear;
		Scale555To555 = BilinearScalerInternal<
				uint16, Manip555to555, uint16>::ScaleBilinear;
		Scale565To565 = BilinearScalerInternal<
				uint16, Manip565to565, uint16>::ScaleBilinear;
#endif
	}

	uint_fast32_t BilinearScaler::ScaleBits() const {
		return 0xFFFFFFFF;
	}

	bool BilinearScaler::ScaleArbitrary() const {
		return true;
	}

	const char* BilinearScaler::ScalerName() const {
		return "bilinear";
	}

	const char* BilinearScaler::ScalerDesc() const {
		return "Bilinear Filtering Scaler";
	}

	const char* BilinearScaler::ScalerCopyright() const {
		return "Copyright (C) 2005 The Pentagram Team, 2010 The Exult Team";
	}

	int BilinearScaler::granularity() const {
		return 4;
	}

}    // namespace Pentagram
