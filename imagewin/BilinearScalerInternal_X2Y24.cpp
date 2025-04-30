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

	// Scale incoming 6x2 block of pixels to 2x12
	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	BSI_FORCE_INLINE void ScaleBlock2x12(
			const uint8* const a, const uint8* const b, const uint8* const c,
			const uint8* const d, const uint8* const e, const uint8* const f,
			const uint8* const g, const uint8* const h, const uint8* const i,
			const uint8* const j, const uint8* const k, const uint8* const l,
			uint8*& pixel,
			const uint_fast32_t pitch, const limit_t limit = nullptr) {

		ScaleBlock2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128,  256, pixel, pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128, (0x500 * 11 / 12) & 0xff,
				pixel, pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128, (0x500 * 10 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				b, c, h, i, 256, 128, (0x500 * 9 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				b, c, h, i, 256, 128, (0x500 * 8 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 7 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 6 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 5 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				d, e, j, k, 256, 128, (0x500 * 4 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				d, e, j, k, 256, 128, (0x500 * 3 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				e, f, k, l, 256, 128, (0x500 * 2 / 12) & 0xff, pixel,
				pitch, limit);
		ScaleBlock2x1<uintX, Manip, uintS>(
				e, f, k, l, 256, 128, (0x500 * 1 / 12) & 0xff, pixel,
				pitch, limit);
	}
	#if 0
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
#else
	// 2x Blinear Scaler
	// 2x scaler is a specialization of Arb. It works almost identically to Arb
	// But instead of using ScaleBlockArb it uses Scale2x2Block_2x
	// without fixed point values used to calculate the filtering coefficents
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X2Y24(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_w = tex->w, tex_h = tex->h;
		// sw &= ~3;
		const uint_fast8_t blockwidth  = 2;
		const uint_fast8_t blockheight = 5;
		// Number of times yloop can run.
		// this is the number of 4 line blocks we can safely scale without
		// checking for buffer overflow
		const uint_fast32_t numyloops = ((sh - 1) / blockheight);

		// Source buffer pointers
		const int    tpitch = tex->pitch / sizeof(uintS);
		const uintS* texel
				= static_cast<uintS*>(tex->pixels) + (sy * tpitch + sx);
		int          numxloops = (sw - 1) / blockwidth;
		const uintS* xloop_end = texel + 1 + (numxloops * blockwidth);
		const uintS* yloop_end = texel + (numyloops * blockheight) * tpitch;

		// Absolute limit of the source buffer. Must not read at or beyondthis
		const uintS* srclimit
				= static_cast<uintS*>(tex->pixels) + (tex_h * tpitch);
		int tex_diff = (tpitch * blockheight) - sw;

		// 2*6 Source RGBA Pixel block being scaled RGBA. Alpha values are
		// currently ignored abcdef are a column and ghijkl are the other column
		// Column can be used in either order
		uint8 a[4] = "A";
		uint8 b[4] = "B";
		uint8 c[4] = "C";
		uint8 d[4] = "D";
		uint8 e[4] = "E";
		uint8 f[4] = "F";
	
		uint8 g[4] = "G";
		uint8 h[4] = "H";
		uint8 i[4] = "I";
		uint8 j[4] = "J";
		uint8 k[4] = "K";
		uint8 l[4] = "L";

		// Absolute limit of dest buffer. Must not write beyond this
		uint8* const dst_limit = pixel + dh * pitch;

		// if no clipping is requested only disable clipping x if the source
		// actually has the neeeded width to safely read the line unclipped
		// Odd widths always need clipping
		bool clip_x = true;
		if ((sx + 3 + numxloops * blockwidth) < tex_w && !clamp_src
			&& !(sw & 1)) {
			clip_x = false;
			numxloops++;
			xloop_end = texel + 1 + (numxloops * blockwidth);
			tex_diff--;
		}

		// clip_y
		bool clip_y = true;
		// if request no clamping, check to see if y remains in the bounds of
		// the texture. If it does we can dissable clipping on y
		if (sh + sy < tex_h && !clamp_src && (sh & 3) == 0) {
			clip_y    = false;
			yloop_end = texel + (sh)*tpitch;    // Stop the yloop at the actual
												// source height
		}

		// Check if enough lines for loop. if not then set clip_y and prevent
		// loop Must have 5 lines to do loop
		if (texel + 5 * tpitch > srclimit) {
			yloop_end = texel;
			clip_y    = true;
		}
		// Src Loop Y
		while (texel != yloop_end) {
			if (texel > yloop_end) {
				return true;
			}
			// Read first column of 5 lines into abcde
			ReadTexelsV<Manip>(blockheight+1, texel, tpitch, a, b, c, d, e, f);
			// advance texel pointer by 1 to the next column
			texel++;

			// Src Loop X, loops while there are 2 or more columns available
			// auto xdiff = xloop_end - (texel + numxloops * blockwidth);
			// xloop_end  = texel + (numxloops * blockwidth);
			assert(xloop_end == (texel + numxloops * blockwidth));
			while (texel != xloop_end) {
				// Read next column of 5 lines into fghij
				ReadTexelsV<Manip>(blockheight+1, texel, tpitch, g, h, i, j, k, l);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing abcde as left and just read fghij
				// as right
				// Generate all dest pixels for The 4 inputsource pixels

				ScaleBlock2x12<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);


				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;

				// Read next column of 5 lines into abcde
				// Keeping existing fghij from above
				ReadTexelsV<Manip>(blockheight+1, texel, tpitch, a, b, c, d, e, f);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing fghij as left and just read abcde
				// as right Generate all dest pixels for The 4 input source
				// pixels


				ScaleBlock2x12<uintX, Manip, uintS>(g,h,i,j,k,l,a,b,c,d,e,f, pixel, pitch,dst_limit);

				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;
			}
			//	assert(cols == numxloops);

			// Final X (clipping) if  have a source column available
			if (clip_x) {
				// Read last column of 5 lines into fghij
				// if source width is odd we reread the previous column
				if (sw & 1) {
					texel--;
				}
				ReadTexelsV<Manip>(blockheight+1, texel, tpitch, g, h, i, j, k, l);
				texel++;

				// Interpolate abcde as left and fghij as right
				//
				ScaleBlock2x12<uintX, Manip, uintS>(a,b,c,d,e,f,g,h,i,j,k,l, pixel, pitch,dst_limit);

				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;

				// odd widths do not need the second column
				if (!(sw & 1)) {

					ScaleBlock2x12<uintX, Manip, uintS>(
							g,h,i,j,k,l,g,h,i,j,k,l, pixel, pitch, dst_limit);

					pixel -= pitch * 12;
					pixel += sizeof(uintX) * 2;
				}
			}
			pixel += (pitch * 12) - (dw * sizeof(uintX));

			texel += tex_diff;
			xloop_end += tpitch * blockheight;
		}

		//
		// Final Rows - Clipped to height
		//
		if (clip_y) {
			uint_fast8_t clipping = sh % blockheight;
			if (clipping == 0) clipping = blockheight;
			
			// If no clamping was requested and we have pixels available, allow
			// reading beyond the source rect
			if (numyloops * blockheight + clipping + sy < tex_h && !clamp_src) {
				// This should never be more than 4 but it doesn't matter if it
				// is
				clipping = std::max<uint_fast16_t>(
						255, tex_h - sy - numyloops * blockheight);
			}

			// Read column 0 of block but clipped to clipping lines
			a[0] = 0xff;
			a[1] = 0;
			a[2] = 0;
			ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e, f);
			texel++;

			// Src Loop X
			while (texel != xloop_end) {
				ReadTexelsV<Manip>(clipping, texel, tpitch, g, h, i, j, k, l);
				texel++;

				ScaleBlock2x12<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);

				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;

				// Read5_Clipped(a, b, c, d, e, 4);
				a[0] = 0;
				a[1] = 0xff;
				a[2] = 0;
				ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e, f);
				texel++;

				ScaleBlock2x12<uintX, Manip, uintS>(
						g, h, i, j, k, l, a, b, c, d, e, f, pixel, pitch,
						dst_limit);

				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;
			};

			// Final X (clipping) if have a source column available
			// this happens when sw is even or 1
			//
			if (clip_x) {
				// Read last column of 5 lines into fghij
				if (sw & 1) {
					// if source width is 1 go back a column so we re-read the
					// column and do not exceed bounds
					texel--;
				}
				ReadTexelsV<Manip>(clipping, texel, tpitch, g, h, i, j, k, l);
				texel++;

				ScaleBlock2x12<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);


				pixel -= pitch * 12;
				pixel += sizeof(uintX) * 2;

				if (!(sw & 1)) {
					ScaleBlock2x12<uintX, Manip, uintS>(
							g, h, i, j, k, l, g, h, i, j, k, l, pixel, pitch,
							dst_limit);

				}
			}
		}

		return true;
	}
#endif 
	InstantiateBilinearScalerFunc(BilinearScalerInternal_X2Y24);

}}    // namespace Pentagram::nsBilinearScaler
