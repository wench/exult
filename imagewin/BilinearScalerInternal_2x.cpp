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
#include "ignore_unused_variable_warning.h"
#include "manip.h"

namespace Pentagram { namespace nsBilinearScaler {

	// 2x Blinear Scaler
	// 2x scaler is a specialization of Arb. It works almost identically to Arb
	// but uses hardcoded filtering coefficients for 2x scaling
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_2x(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		// Number of times yloop can run.
		// this is the number of 4 line blocks we can safely scale without
		// checking for buffer overflow
		const int numyloops = ((sh - 1) / 4);

		// Source buffer pointers
		const uint_fast32_t tpitch = tex->pitch / sizeof(uintS);
		const uintS*        texel
				= static_cast<uintS*>(tex->pixels) + (sy * tpitch + sx);
		const uintS* xloop_end = texel + (sw - 1);
		const uintS* yloop_end = texel + (numyloops * 4) * tpitch;

		// Absolute limit of the source buffer. Must not read beyond this
		const uintS* srclimit
				= static_cast<uintS*>(tex->pixels) + (tex->h * tpitch);
		int tex_diff = (tpitch * 4) - sw;

		uint8     a[4];
		uint8     b[4];
		uint8     c[4];
		uint8     d[4];
		uint8     e[4];
		uint8     f[4];
		uint8     g[4];
		uint8     h[4];
		uint8     i[4];
		uint8     j[4];
		const int p_diff = (pitch * 8) - (dw * sizeof(uintX));

		// Absolute limit of dest buffer. Must not write beyond this
		const uint8* dst_limit = pixel + dh * pitch;

		bool clip_x = true;
		if (sw + sx < tpitch && !clamp_src) {
			clip_x    = false;
			xloop_end = texel + (sw + 1);
			tex_diff--;
		}

		bool clip_y = true;
		if (sh + sy < static_cast<unsigned int>(tex->h) && !clamp_src) {
			clip_y    = false;
			yloop_end = texel + (sh)*tpitch;
		}

		// Check if enough lines for loop. if not then set clip_y and prevent
		// loop Must have 5 lines to do loop
		if (texel + 5 * tpitch > srclimit) {
			yloop_end = texel;
			clip_y    = true;
		}
		// Src Loop Y
		while (texel != yloop_end) {
			ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
			texel++;

			// Src Loop X
			do {
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				texel++;

				ScalePixel2x(a, b, f, g);
				ScalePixel2x(b, c, g, h);
				ScalePixel2x(c, d, h, i);
				ScalePixel2x(d, e, i, j);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
				texel++;

				ScalePixel2x(f, g, a, b);
				ScalePixel2x(g, h, b, c);
				ScalePixel2x(h, i, c, d);
				ScalePixel2x(i, j, d, e);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

			} while (texel != xloop_end);

			// Final X (clipping)
			if (clip_x) {
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				texel++;

				ScalePixel2x(a, b, f, g);
				ScalePixel2x(b, c, g, h);
				ScalePixel2x(c, d, h, i);
				ScalePixel2x(d, e, i, j);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				ScalePixel2x(f, g, f, g);
				ScalePixel2x(g, h, g, h);
				ScalePixel2x(h, i, h, i);
				ScalePixel2x(i, j, i, j);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;
			}

			pixel += p_diff;

			texel += tex_diff;
			xloop_end += tpitch * 4;
		}

		//
		// Final Rows - Clipped to height
		//
		// We don't need to keep track of how many lines have been scaled as the
		// number is always a multple of 4 so sh mod 4 is number unscaled. but
		// if clip_y and 0 lines remaining there are actually still 4 remaining
		if (clip_y && (sh & 3) == 0) {
			// Read 5 lines but clipped to only 4
			ReadTexelsV<Manip>(4, texel, tpitch, a, b, c, d, e);
			texel++;

			// Src Loop X
			do {
				ReadTexelsV<Manip>(4, texel, tpitch, f, g, h, i, j);
				texel++;
				ScalePixel2x(a, b, f, g);
				ScalePixel2x(b, c, g, h);
				ScalePixel2x(c, d, h, i);
				ScalePixel2x(d, e, i, j);
				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				ReadTexelsV<Manip>(4, texel, tpitch, a, b, c, d, e);
				texel++;
				ScalePixel2x(f, g, a, b);
				ScalePixel2x(g, h, b, c);
				ScalePixel2x(h, i, c, d);
				ScalePixel2x(i, j, d, e);
				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;
			} while (texel != xloop_end);

			// Final X (clipping)
			if (clip_x) {
				ReadTexelsV<Manip>(4, texel, tpitch, f, g, h, i, j);
				texel++;

				ScalePixel2x(a, b, f, g);
				ScalePixel2x(b, c, g, h);
				ScalePixel2x(c, d, h, i);
				ScalePixel2x(d, e, i, j);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				ScalePixel2x(f, g, f, g);
				ScalePixel2x(g, h, g, h);
				ScalePixel2x(h, i, h, i);
				ScalePixel2x(i, j, i, j);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;
			}

			pixel += p_diff;

			texel += tex_diff;
			xloop_end += tpitch * 4;
		} else if (clip_y && (sh & 3)) {
			// Height is Non multiple of 4
			return false;
		}
		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_2x);

}}    // namespace Pentagram::nsBilinearScaler
