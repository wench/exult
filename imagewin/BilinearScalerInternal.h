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

#include "BilinearScaler.h"
#include "manip.h"

#include <cstddef>
#include <cstring>
#include <type_traits>
#define COMPILE_ALL_BILINEAR_SCALERS
#ifdef _MSC_VER
#	define BSI_FORCE_INLINE __forceinline
#elif defined(__clang__)
#	define BSI_FORCE_INLINE [[clang::always_inline]] inline
#elif defined __GNUC__
#	define BSI_FORCE_INLINE [[gnu::always_inline]] inline
#else
#	define BSI_FORCE_INLINE inline
#endif
namespace Pentagram { namespace nsBilinearScaler {

	typedef uint_fast32_t fixedu1616;
/*
	template <typename limit_t = std::nullptr_t>
	BSI_FORCE_INLINE bool IsUnclipped(uint8* dest, limit_t limit = nullptr) {
		return std::is_null_pointer<limit_t>::value
			   || dest < static_cast<uint8*>(limit);
	}

	template <typename uintX, typename limit_t = std::nullptr_t>
	BSI_FORCE_INLINE void WritePix(
			uint8* dest, uintX val, limit_t limit = nullptr) {
		if (IsUnclipped(dest, limit)) {
			std::memcpy(dest, &val, sizeof(uintX));
		}
	}
*/
	template <typename limit_t = std::nullptr_t>
	BSI_FORCE_INLINE bool IsUnclipped(uint8* dest, limit_t limit = nullptr)
	{
		return std::is_null_pointer<limit_t>::value || dest < static_cast<uint8*>(limit);
	}

	template <typename uintX, typename limit_t = std::nullptr_t>
	BSI_FORCE_INLINE void WritePix(
			uint8* dest, uintX val, limit_t limit = nullptr) {
		if (IsUnclipped(dest,limit)) {
			std::memcpy(dest, &val, sizeof(uintX));
		}
	}
	BSI_FORCE_INLINE uint_fast32_t
			SimpleLerp(uint_fast32_t a, uint_fast32_t b, uint_fast32_t fac) {
		return (b << 8) + (a - b) * (fac);
	}

	BSI_FORCE_INLINE uint_fast32_t
			SimpleLerp2(uint_fast32_t a, uint_fast32_t b, uint_fast32_t fac) {
		return (b << 16) + (a - (b)) * (fac);
	}

#define CopyLerp(d, a, b, f)                       \
	do {                                           \
		(d)[0] = SimpleLerp2(b[0], a[0], f) >> 16; \
		(d)[1] = SimpleLerp2(b[1], a[1], f) >> 16; \
		(d)[2] = SimpleLerp2(b[2], a[2], f) >> 16; \
	} while (false)

#define FilterPixelM(a, b, f, g, fx, fy, limit)                  \
	do {                                                        \
		WritePix<uintX>(                                        \
				pixel,                                          \
				Manip::rgb(                                     \
						SimpleLerp(                             \
								SimpleLerp(a[0], f[0], fx),     \
								SimpleLerp(b[0], g[0], fx), fy) \
								>> 16,                          \
						SimpleLerp(                             \
								SimpleLerp(a[1], f[1], fx),     \
								SimpleLerp(b[1], g[1], fx), fy) \
								>> 16,                          \
						SimpleLerp(                             \
								SimpleLerp(a[2], f[2], fx),     \
								SimpleLerp(b[2], g[2], fx), fy) \
								>> 16),                         \
				limit);                                         \
	} while (false)

#define ScalePixel2xClipped(a, b, f, g, limit)                       \
	do {                                                             \
		WritePix<uintX>(pixel, Manip::rgb(a[0], a[1], a[2]), limit); \
		WritePix<uintX>(                                             \
				pixel + sizeof(uintX),                               \
				Manip::rgb(                                          \
						(a[0] + f[0]) >> 1, (a[1] + f[1]) >> 1,      \
						(a[2] + f[2]) >> 1),                         \
				limit);                                              \
		pixel += pitch;                                              \
		WritePix<uintX>(                                             \
				pixel,                                               \
				Manip::rgb(                                          \
						(a[0] + b[0]) >> 1, (a[1] + b[1]) >> 1,      \
						(a[2] + b[2]) >> 1),                         \
				limit);                                              \
		WritePix<uintX>(                                             \
				pixel + sizeof(uintX),                               \
				Manip::rgb(                                          \
						(a[0] + b[0] + f[0] + g[0]) >> 2,            \
						(a[1] + b[1] + f[1] + g[1]) >> 2,            \
						(a[2] + b[2] + f[2] + g[2]) >> 2),           \
				limit);                                              \
		pixel += pitch;                                              \
	} while (false)
#define ScalePixel2x(a, b, f, g) ScalePixel2xClipped(a, b, f, g, nullptr)

#define X2Xy24xLerps(c0, c1, y)                                              \
	do {                                                                     \
		WritePix<uintX>(                                                     \
				pixel,                                                       \
				Manip::rgb(cols[c0][y][0], cols[c0][y][1], cols[c0][y][2])); \
		WritePix<uintX>(                                                     \
				pixel + sizeof(uintX),                                       \
				Manip::rgb(                                                  \
						(cols[c0][y][0] + cols[c1][y][0]) >> 1,              \
						(cols[c0][y][1] + cols[c1][y][1]) >> 1,              \
						(cols[c0][y][2] + cols[c1][y][2]) >> 1));            \
	} while (false)

#define X2xY24xInnerLoop(c0, c1)  \
	do {                          \
		X2Xy24xLerps(c0, c1, 0);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 1);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 2);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 3);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 4);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 5);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 6);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 7);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 8);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 9);  \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 10); \
		pixel += pitch;           \
		X2Xy24xLerps(c0, c1, 11); \
		pixel += pitch;           \
	} while (false)

#define X2xY24xDoColsA()                     \
	do {                                     \
		CopyLerp(cols[0][0], a, b, 0x0000);  \
		CopyLerp(cols[0][1], a, b, 0x6AAA);  \
		CopyLerp(cols[0][2], a, b, 0xD554);  \
		CopyLerp(cols[0][3], b, c, 0x3FFE);  \
		CopyLerp(cols[0][4], b, c, 0xAAA8);  \
		CopyLerp(cols[0][5], c, d, 0x1552);  \
		CopyLerp(cols[0][6], c, d, 0x7FFC);  \
		CopyLerp(cols[0][7], c, d, 0xEAA6);  \
		CopyLerp(cols[0][8], d, e, 0x5550);  \
		CopyLerp(cols[0][9], d, e, 0xBFFA);  \
		CopyLerp(cols[0][10], e, l, 0x2AA4); \
		CopyLerp(cols[0][11], e, l, 0x954E); \
	} while (false)

#define X2xY24xDoColsB()                     \
	do {                                     \
		CopyLerp(cols[1][0], f, g, 0x0000);  \
		CopyLerp(cols[1][1], f, g, 0x6AAA);  \
		CopyLerp(cols[1][2], f, g, 0xD554);  \
		CopyLerp(cols[1][3], g, h, 0x3FFE);  \
		CopyLerp(cols[1][4], g, h, 0xAAA8);  \
		CopyLerp(cols[1][5], h, i, 0x1552);  \
		CopyLerp(cols[1][6], h, i, 0x7FFC);  \
		CopyLerp(cols[1][7], h, i, 0xEAA6);  \
		CopyLerp(cols[1][8], i, j, 0x5550);  \
		CopyLerp(cols[1][9], i, j, 0xBFFA);  \
		CopyLerp(cols[1][10], j, k, 0x2AA4); \
		CopyLerp(cols[1][11], j, k, 0x954E); \
	} while (false)

#define X1xY12xCopy(y) \
	WritePix<uintX>(pixel, Manip::rgb(cols[y][0], cols[y][1], cols[y][2]))

#define X1xY12xInnerLoop() \
	do {                   \
		X1xY12xCopy(0);    \
		pixel += pitch;    \
		X1xY12xCopy(1);    \
		pixel += pitch;    \
		X1xY12xCopy(2);    \
		pixel += pitch;    \
		X1xY12xCopy(3);    \
		pixel += pitch;    \
		X1xY12xCopy(4);    \
		pixel += pitch;    \
		X1xY12xCopy(5);    \
		pixel += pitch;    \
	} while (false)

#define X1xY12xDoCols()                  \
	do {                                 \
		CopyLerp(cols[0], a, b, 0x0000); \
		CopyLerp(cols[1], a, b, 0xD554); \
		CopyLerp(cols[2], b, c, 0xAAA8); \
		CopyLerp(cols[3], c, d, 0x7FFC); \
		CopyLerp(cols[4], d, e, 0x5550); \
		CopyLerp(cols[5], e, l, 0x2AA4); \
	} while (false)



#if 0 
 #define Read6_Clipped(a, b, c, d, e, l, count)                            \
	do {                                                                  \
		Read5_Clipped(a, b, c, d, e, count);                              \
		if (count >= 6) {                                                 \
			Manip::split_source(*(texel + tpitch * 5), l[0], l[1], l[2]); \
		} else {                                                          \
			l[0] = e[0];                                                  \
			l[1] = e[1];                                                  \
			l[2] = e[2];                                                  \
		}                                                                 \
	} while (false)
#else
#define Read6_Clipped(a, b, c, d, e, f, count) ReadTexelsV<Manip>(count, texel,tpitch,  a, b, c, d, e, f)
#endif
#define Read6(a, b, c, d, e, l) Read6_Clipped(a, b, c, d, e, l, 6)

	// Read 1 texel
	template <class Manip, typename texel_t, typename Ta>
	BSI_FORCE_INLINE void ReadTexelsV(
			const uint_fast8_t clipping, texel_t* texel, size_t, Ta& a) {
		// Read a if we can
		if (clipping >= 1) {
			Manip::split_source(*texel, a[0], a[1], a[2]);
		}
	}

	// Read 2 or more texels Vertically with a constant clipping value.
	// If all texels are clipped a should be set to a reasonable default value
	// before calling this as it will be copied to all other texels
	template <
			class Manip, typename texel_t, typename Ta, typename Tb,
			typename... Args>
	BSI_FORCE_INLINE void ReadTexelsV(
			const uint_fast8_t clipping, texel_t* texel, size_t tpitch, Ta &a,
			Tb &b, Args&... more) {
		// Read a
		ReadTexelsV<Manip>(clipping, texel, tpitch, a);

		// Copy a to b if needed
		if (clipping < 2) {
			b[0] = a[0];
			b[1] = a[1];
			b[2] = a[2];
			//  recursive call to copy b to the rest of the args
			ReadTexelsV<Manip>(0, texel + tpitch, tpitch, b, more...);
		}
		// Recursive call to read b and the rest of the args
		else {
			ReadTexelsV<Manip>(
					clipping - 1, texel + tpitch, tpitch, b, more...);
		}
	}

	// Bilinear scaler specialized for 2x scaling only
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_2x(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Bilinear Scaler specialized for 2x horizontal 2.4x vertical scaling aka
	// aspect correct 2x. sh must be a multiple of 5
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X2Y24(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Bilinear Scaler specialized for 1x horizontal 1.2x vertical scaling aka
	// aspect correction with no scaling.
	// sh must be a multiple of 5
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X1Y12(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Arbitrary Bilinrat scaler capble of scaling by any finteger or non
	// integer factoe with no restrictions
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_Arb(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

#ifdef COMPILE_GAMMA_CORRECT_SCALERS
#	define InstantiateFunc(func, a, b, c)                                 \
		template bool func<a, b, c>(                                       \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool);                                      \
		template bool func<a, b##_GC, c>(                                  \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool)
#else
#	define InstantiateFunc(func, a, b, c)                                 \
		template bool func<a, b, c>(                                       \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool)
#endif

#ifdef COMPILE_ALL_BILINEAR_SCALERS
#	define InstantiateBilinearScalerFunc(func)               \
		InstantiateFunc(func, uint32, Manip8to32, uint8);     \
		InstantiateFunc(func, uint32, Manip32to32, uint32);   \
		InstantiateFunc(func, uint16, Manip8to565, uint8);    \
		InstantiateFunc(func, uint16, Manip8to16, uint8);     \
		InstantiateFunc(func, uint16, Manip8to555, uint8);    \
		InstantiateFunc(func, uint16, Manip16to16, uint16);   \
		InstantiateFunc(func, uint16, Manip555to555, uint16); \
		InstantiateFunc(func, uint16, Manip565to565, uint16)
#else
#	define InstantiateBilinearScalerFunc(func)             \
		InstantiateFunc(func, uint32_t, Manip8to32, uint8); \
		InstantiateFunc(func, uint32_t, Manip32to32, uint32_t)
#endif
}}    // namespace Pentagram::nsBilinearScaler
