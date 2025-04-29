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

#include "BilinearScalerInternal.h"
#include "manip.h"

namespace Pentagram { namespace BilinearScaler {

	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X2Y24(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		ignore_unused_variable_warning(dh);
		// Height must be greater than 5 and a multiple of 5
		if (sh < 5 || sh & 5) {
			return false;
		}    // Source buffer pointers
		const uint_fast32_t tpitch = tex->pitch / sizeof(uintS);
		uintS* texel = static_cast<uintS*>(tex->pixels) + (sy * tpitch + sx);
		uintS* tline_end = texel + (sw - 1);
		uintS* tex_end   = texel + (sh - 5) * tpitch;
		int    tex_diff  = (tpitch * 5) - sw;

		// 2x5 Block of RGBA Source Pixels
		uint8 a[4];
		uint8 b[4];
		uint8 c[4];
		uint8 d[4];
		uint8 e[4];
		uint8 f[4];
		uint8 g[4];
		uint8 h[4];
		uint8 i[4];
		uint8 j[4];
		uint8 k[4];
		uint8 l[4];
		// Work Buffer for Block of 2x12 scaled pixels.
		uint8 cols[2][12][4];

		bool clip_x = true;
		if (sw + sx < tpitch && !clamp_src) {
			clip_x    = false;
			tline_end = texel + (sw + 1);
			tex_diff--;
		}

		bool clip_y = true;
		if (sh + sy < static_cast<unsigned int>(tex->h) && !clamp_src) {
			clip_y  = false;
			tex_end = texel + (sh)*tpitch;
		}

		// Src Loop Y
		while (texel < tex_end) {
			// Read first column of pixels
			Read6(a, b, c, d, e, l);
			texel++;

			// scale first column of pixels into work buffer
			X2xY24xDoColsA();

			// Src Loop X
			do {
				// Read second colum of pixels
				Read6(f, g, h, i, j, k);
				texel++;

				// scale second column of pixels into work buffer
				X2xY24xDoColsB();
				// Interpolate workbuffer horizontally into destination buffer.
				// Treating column 0 as left and column 1 as right
				X2xY24xInnerLoop(0, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;

				// Read a new column of pixels over first column
				Read6(a, b, c, d, e, l);
				texel++;

				// scale first column of pixels into work buffer
				X2xY24xDoColsA();
				// Interpolate workbuffer horizontally into destination buffer.
				// Treating column 1 as left and column 0 as right
				X2xY24xInnerLoop(1, 0);
				pixel -= pitch * 12 - sizeof(uintX) * 2;
			} while (texel != tline_end);

			// Final X (clipping)
			if (clip_x) {
				Read6(f, g, h, i, j, k);
				texel++;

				X2xY24xDoColsB();
				X2xY24xInnerLoop(0, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;

				X2xY24xInnerLoop(1, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;
			}

			pixel += pitch * 12 - sizeof(uintX) * (dw);
			texel += tex_diff;
			tline_end += tpitch * 5;
		}

		//
		// Final Rows - Clipping
		//

		// Src Loop Y
		if (clip_y) {
			Read6_Clipped(a, b, c, d, e, l, 5);
			texel++;

			X2xY24xDoColsA();

			// Src Loop X
			do {
				Read6_Clipped(f, g, h, i, j, k, 5);
				texel++;

				X2xY24xDoColsB();
				X2xY24xInnerLoop(0, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;

				Read6_Clipped(a, b, c, d, e, l, 5);
				texel++;

				X2xY24xDoColsA();
				X2xY24xInnerLoop(1, 0);
				pixel -= pitch * 12 - sizeof(uintX) * 2;
			} while (texel != tline_end);

			// Final X (clipping)
			if (clip_x) {
				Read6_Clipped(f, g, h, i, j, k, 5);
				texel++;

				X2xY24xDoColsB();

				X2xY24xInnerLoop(0, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;

				X2xY24xInnerLoop(1, 1);
				pixel -= pitch * 12 - sizeof(uintX) * 2;
			}
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_X2Y24);

}}    // namespace Pentagram::nsBilinearScaler
