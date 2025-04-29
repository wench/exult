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

namespace Pentagram { namespace BilinearScaler {


	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	// This is mostly an optimized specialization of Scale2x2Block_Arb
	// with unrolled loops and hardcoded filtering coefficients
	BSI_FORCE_INLINE void Scale2x2Block2x(
			const uint8* const tl, const uint8* const bl, const uint8* const tr,
			const uint8* const br, uint8*& pixel, const uint_fast32_t pitch, 
		const limit_t limit = nullptr) {                                                             
	WritePix<uintX>(pixel, Manip::rgb(tl[0], tl[1], tl[2]), limit); 
	WritePix<uintX>(                                             
			pixel + sizeof(uintX),                               
			Manip::rgb(                                          
					(tl[0] + tr[0]) >> 1, (tl[1] + tr[1]) >> 1,      
					(tl[2] + tr[2]) >> 1),                         
			limit);                                              
	pixel += pitch;                                              
	WritePix<uintX>(                                             
			pixel,                                               
			Manip::rgb(                                          
					(tl[0] + bl[0]) >> 1, (tl[1] + bl[1]) >> 1,      
					(tl[2] + bl[2]) >> 1),                         
			limit);                                              
	WritePix<uintX>(                                             
			pixel + sizeof(uintX),                               
			Manip::rgb(                                          
					(tl[0] + bl[0] + tr[0] + br[0]) >> 2,            
					(tl[1] + bl[1] + tr[1] + br[1]) >> 2,            
					(tl[2] + bl[2] + tr[2] + br[2]) >> 2),           
			limit);                                              
	pixel += pitch;                                              
} 
	// 2x Blinear Scaler
	// 2x scaler is a specialization of Arb. It works almost identically to Arb
	// But instead of using Scale2x2Block_Arb it uses Scale2x2Block_2x 
	// without fixed point values used to calculate the filtering coefficents
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_2x(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_w = tex->w, tex_h = tex->h;
		// sw &= ~3;
		const uint_fast8_t blockwidth  = 2;
		const uint_fast8_t blockheight = 4;
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
		int tex_diff = (tpitch * 4) - sw;

		// 2*5 Source RGBA Pixel block being scaled RGBA. Alpha values are
		// currently ignored abcde are a column and fghij are the other column
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
				return false;
			}
			// Read first column of 5 lines into abcde
			ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
			// advance texel pointer by 1 to the next column
			texel++;

			// Src Loop X, loops while there are 2 or more columns available
			// auto xdiff = xloop_end - (texel + numxloops * blockwidth);
			// xloop_end  = texel + (numxloops * blockwidth);
			assert(xloop_end == (texel + numxloops * blockwidth));
			while (texel != xloop_end) {

				// Read next column of 5 lines into fghij
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing abcde as left and just read fghij
				// as right
				// Generate all dest pixels for The 4 inputsource pixels

				Scale2x2Block2x<uintX, Manip, uintS>(a, b, f, g, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(b, c, g, h, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(c, d, h, i, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(d, e, i, j, pixel, pitch);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				// Read next column of 5 lines into abcde
				// Keeping existing fghij from above
				ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
				// advance texel pointer by 1 to the next column
				texel++;


				// Interpolate with existing fghij as left and just read abcde
				// as right Generate all dest pixels for The 4 input source
				// pixels

				Scale2x2Block2x<uintX, Manip, uintS>(f, g, a, b, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(g, h, b, c, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(h, i, c, d, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(i, j, d, e, pixel, pitch);

				pixel -= pitch * 8;
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
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				texel++;


				// Interpolate abcde as left and fghij as right
				//
				Scale2x2Block2x<uintX, Manip, uintS>(a, b, f, g, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(b, c, g, h, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(c, d, h, i, pixel, pitch);
				Scale2x2Block2x<uintX, Manip, uintS>(d, e, i, j, pixel, pitch);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				// odd widths do not need the second column
				if (!(sw & 1)) {
					Scale2x2Block2x<uintX, Manip, uintS>(
							f, g, f, g, pixel, pitch);
					Scale2x2Block2x<uintX, Manip, uintS>(
							g, h, g, h, pixel, pitch);
					Scale2x2Block2x<uintX, Manip, uintS>(
							h, i, h, i, pixel, pitch);
					Scale2x2Block2x<uintX, Manip, uintS>(
							i, j, i, j, pixel, pitch);

					pixel -= pitch * 8;
					pixel += sizeof(uintX) * 2;
				}
			}
			pixel += (pitch * 8) - (dw * sizeof(uintX));

			texel += tex_diff;
			xloop_end += tpitch * 4;
		}

		//
		// Final Rows - Clipped to height
		//
		if (clip_y) {
			// Complex way to do (sh&3)==0 ? 4 : sh&3 without using a
			// conditional just intrger arithmatic and bitwise operations
			uint_fast8_t clipping = 4 - (((sh + 3) ^ 3) & 3);
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
			ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e);
			texel++;

			// Src Loop X
			while (texel != xloop_end) {
				ReadTexelsV<Manip>(clipping, texel, tpitch, f, g, h, i, j);
				texel++;


				Scale2x2Block2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch, dst_limit);
				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				// Read5_Clipped(a, b, c, d, e, 4);
				a[0] = 0;
				a[1] = 0xff;
				a[2] = 0;
				ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e);
				texel++;

				Scale2x2Block2x<uintX, Manip, uintS>(
						f, g, a, b, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						g, h, b, c, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						h, i, c, d, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						i, j, d, e, pixel, pitch, dst_limit);
				pixel -= pitch * 8;
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
				ReadTexelsV<Manip>(clipping, texel, tpitch, f, g, h, i, j);
				texel++;

				Scale2x2Block2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch, dst_limit);
				Scale2x2Block2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch, dst_limit);

				pixel -= pitch * 8;
				pixel += sizeof(uintX) * 2;

				if (!(sw & 1)) {
					Scale2x2Block2x<uintX, Manip, uintS>(
							f, g, f, g, pixel, pitch, dst_limit);
					Scale2x2Block2x<uintX, Manip, uintS>(
							g, h, g, h, pixel, pitch, dst_limit);
					Scale2x2Block2x<uintX, Manip, uintS>(
							h, i, h, i, pixel, pitch, dst_limit);
					Scale2x2Block2x<uintX, Manip, uintS>(
							i, j, i, j, pixel, pitch, dst_limit);

				}
			}

		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_2x);

}}    // namespace Pentagram::nsBilinearScaler
