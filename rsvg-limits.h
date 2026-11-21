/*
   rsvg-limits.h: Resource-limit numbers for untrusted SVG input.

   Copyright (C) 2026 Randy Butler

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, see LICENCE-LIB.
*/

#ifndef RSVG_LIMITS_H
#define RSVG_LIMITS_H

#include <glib.h>

/* Untrusted input. Change a number here; call sites use these names.
 *
 *   Cap                 Default     With RSVG_HANDLE_FLAG_UNLIMITED
 *   Side length         32767 px    never raised
 *   Decoded pixels      128 MiB     512 MiB
 *   data: URI size      64 MiB      256 MiB
 *   File nesting        20          never raised
 *   Layer nesting       500 / 50    never raised
 *   Cairo coordinates   ±8,388,608  never raised
 *   Morphology radius   500 px      never raised
 *   Turbulence octaves  8           never raised
 */

#define RSVG_MAX_IMAGE_DIMENSION             32767
#define RSVG_MAX_IMAGE_PIXEL_BYTES           ((gsize) 128 * 1024 * 1024)
#define RSVG_MAX_IMAGE_PIXEL_BYTES_UNLIMITED ((gsize) 512 * 1024 * 1024)
#define RSVG_MAX_DATA_URI_BYTES              ((gsize) 64 * 1024 * 1024)
#define RSVG_MAX_DATA_URI_BYTES_UNLIMITED    ((gsize) 256 * 1024 * 1024)
#define RSVG_MAX_FILE_NESTING_DEPTH          20
/* Nested discrete layers. musl's default thread stack is small, so
 * the cap is 50 there; glibc and other libcs use 500. Never raised. */
#if defined(__linux__) && !defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__ANDROID__)
#  define RSVG_MAX_LAYER_NESTING_DEPTH 50
#else
#  define RSVG_MAX_LAYER_NESTING_DEPTH 500
#endif
/* cairo_fixed_t is 24.8; representable range is a bit over ±8 million.
 * Do not pass coordinates outside this to Cairo. Never raised. */
#define RSVG_CAIRO_COORD_MAX ((double) 2147483647 / 256.0)
#define RSVG_CAIRO_COORD_MIN ((double) (-2147483647 - 1) / 256.0)
/* feMorphology neighborhood in device pixels after the primitive affine. */
#define RSVG_MAX_MORPHOLOGY_RADIUS 500
/* feTurbulence octave loop. Each octave doubles frequency. */
#define RSVG_MAX_TURBULENCE_OCTAVES 8

G_STATIC_ASSERT (RSVG_MAX_IMAGE_PIXEL_BYTES_UNLIMITED > RSVG_MAX_IMAGE_PIXEL_BYTES);
G_STATIC_ASSERT (RSVG_MAX_DATA_URI_BYTES_UNLIMITED > RSVG_MAX_DATA_URI_BYTES);

#endif /* RSVG_LIMITS_H */
