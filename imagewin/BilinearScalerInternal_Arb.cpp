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

namespace Pentagram {

	// Arbitrary Bilinear Scaler
	// It works on blocks of 2x5 source pixels at a time. Uses 16.16 Fixed point
	// to calculate filtering coefficients for each destination pixel
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_Arb(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_w = tex->w, tex_h = tex->h;
		sw &= ~3;
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
		const int    numxloops = ((sw & ~1) - 1) / blockwidth;
		const uintS* tline_end = texel + (sw - 1);
		const uintS* xloop_end = texel + 1 + (numxloops * blockwidth);
		const uintS* yloop_end = texel + (numyloops * blockheight) * tpitch;

		// Absolute limit of the source buffer. Must not read at or beyondthis
		const uintS* srclimit
				= static_cast<uintS*>(tex->pixels) + (tex_h * tpitch);
		int tex_diff = (tpitch * 4) - sw;

		// 2*5 Source Pixel block being scaled RGBA but A is currently unused
		// a f
		// b g
		// c h
		// d i
		// e j
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
		const uint8* dst_limit = pixel + dh * pitch;

		// Current Fixed point position of dest pixel in source buffer
		fixedu1616 pos_y = 0;
		fixedu1616 pos_x = 0;

		// Fixedpoint position increments when advancing dest pixels
		const fixedu1616 add_y = (sh << 16) / dh;
		const fixedu1616 add_x = (sw << 16) / dw;

		// Limits for block being worked on
		// start_x and dst_y are calculated backward from the last pixel so the
		// last pixel in each dest block has no fractional component when
		// filtering
		fixedu1616 start_x = (sw << 16) - (add_x * dw);
		fixedu1616 dst_y   = (sh << 16) - (add_y * dh);
		fixedu1616 end_y   = 1 << 16;

		// Offset by half a source pixel when doing 0.5x scaling
		// looks a bit better
		if (sw == dw * 2) {
			start_x += 0x8000;
		}
		if (sh == dh * 2) {
			dst_y += 0x8000;
		}

		uint8* blockline_start = nullptr;
		uint8* next_block      = nullptr;

		// if no clamping is requested only disable clipping if the source rect
		// is actually within the buffer and width is even

		bool clip_x = true;
		if (sw + sx < tex_w && !clamp_src && (sw & 1) == 0) {
			clip_x    = false;
			xloop_end = texel + (sw + 1);
			tline_end += 2;
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
		int blocks = 0;
		int lines  = 0;
		while (texel != yloop_end) {
			auto texstart = texel;
			// Read first 5 lines col 0
			// Read5(a, b, c, d, e);
			ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
			texel++;
			blocks++;
			lines += 4;
			uint_fast32_t end_x = 1 << 16;
			uint_fast32_t dst_x = start_x;

			next_block = pixel;
			// Src Loop X
			int cols = 0;
			/// xloop_end = texel + numxloops * 2;
			do {
				cols++;
				pos_y = dst_y;

				// Read col1
				// Read5(f, g, h, i, j);
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate col0 -> col1
				// a f
				// b g
				ArbInnerLoop(a, b, f, g);
				// b g
				// c h
				ArbInnerLoop(b, c, g, h);
				// c h
				// d i
				ArbInnerLoop(c, d, h, i);
				ArbInnerLoop(d, e, i, j);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
				pos_y = dst_y;

				// Read col0
				ReadTexelsV<Manip>(5, texel, tpitch, a, b, c, d, e);
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate col1 -> col0
				// f a
				// g b
				ArbInnerLoop(f, g, a, b);
				// g b
				// h c
				ArbInnerLoop(g, h, b, c);
				// h c
				// i d
				ArbInnerLoop(h, i, c, d);
				// i d
				// j e
				ArbInnerLoop(i, j, d, e);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
			} while (texel != tline_end);

			// Final X (clipping)
			if (clip_x) {
				pos_y = dst_y;

				// Read col1
				// Read5(f, g, h, i, j);
				ReadTexelsV<Manip>(5, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate col0 -> col1
				ArbInnerLoop(a, b, f, g);
				ArbInnerLoop(b, c, g, h);
				ArbInnerLoop(c, d, h, i);
				ArbInnerLoop(d, e, i, j);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
				pos_y = dst_y;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate col1 -> col1
				ArbInnerLoop(f, g, f, g);
				ArbInnerLoop(g, h, g, h);
				ArbInnerLoop(h, i, h, i);
				ArbInnerLoop(i, j, i, j);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
			}

			pixel += pitch - sizeof(uintX) * (dw);

			dst_y = pos_y;
			end_y += 4 << 16;
			auto diff = texel - texstart;
			texel += tex_diff;
			auto alt = texel + 1 + (numxloops * blockwidth);
			tline_end += tpitch * 4;
			// xloop_end = alt;
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
			ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e);
			texel++;

			uint_fast32_t end_x = 1 << 16;
			uint_fast32_t dst_x = start_x;

			next_block = pixel;

			// Src Loop X
			do {
				pos_y = dst_y;

				// Read5_Clipped(f, g, h, i, j, 4);
				ReadTexelsV<Manip>(clipping, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				ArbInnerLoopClipped(a, b, f, g, dst_limit);
				ArbInnerLoopClipped(b, c, g, h, dst_limit);
				ArbInnerLoopClipped(c, d, h, i, dst_limit);
				ArbInnerLoopClipped(d, e, i, j, dst_limit);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
				pos_y = dst_y;

				// Read5_Clipped(a, b, c, d, e, 4);
				ReadTexelsV<Manip>(clipping, texel, tpitch, a, b, c, d, e);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				ArbInnerLoopClipped(f, g, a, b, dst_limit);
				ArbInnerLoopClipped(g, h, b, c, dst_limit);
				ArbInnerLoopClipped(h, i, c, d, dst_limit);
				ArbInnerLoopClipped(i, j, d, e, dst_limit);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
			} while (texel != tline_end);

			// Final X (clipping)
			if (clip_x) {
				pos_y = dst_y;

				// Read5_Clipped(f, g, h, i, j, 4);
				ReadTexelsV<Manip>(clipping, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				ArbInnerLoopClipped(a, b, f, g, dst_limit);
				ArbInnerLoopClipped(b, c, g, h, dst_limit);
				ArbInnerLoopClipped(c, d, h, i, dst_limit);
				ArbInnerLoopClipped(d, e, i, j, dst_limit);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
				pos_y = dst_y;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				ArbInnerLoopClipped(f, g, f, g, dst_limit);
				ArbInnerLoopClipped(g, h, g, h, dst_limit);
				ArbInnerLoopClipped(h, i, h, i, dst_limit);
				ArbInnerLoopClipped(i, j, i, j, dst_limit);

				end_y -= 4 << 16;
				dst_x = pos_x;
				end_x += 1 << 16;
			}
		} else if (clip_y && (sh & 3)) {
			// Height is Non multiple of 4
			// return false;
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_Arb);

}    // namespace Pentagram
