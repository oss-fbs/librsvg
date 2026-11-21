/*
 * Copyright (C) 2026 Randy Butler
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see LICENCE-LIB.
 *
 * Tiny C++ sample: load an SVG, query size, draw it, write a PNG.
 * Not part of make / make check. See cpp/README.
 *
 * c++ -std=c++11 -I. example.cpp \
 *     $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)
 */

#include "librsvg.hpp"

#include <cairo.h>
#include <iostream>

int
main (void)
{
    try {
        /* Load; Handle unrefs the C object when this block ends. */
        rsvg::Handle svg = rsvg::Handle::from_file ("../examples/icon.svg");
        svg.set_dpi (96.0);

        double px_w = 256.0, px_h = 256.0;
        svg.intrinsic_size_in_pixels (px_w, px_h);

        RsvgRectangle viewport = { 0.0, 0.0, px_w, px_h };
        cairo_surface_t *surface =
            cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (int) px_w, (int) px_h);
        cairo_t *cr = cairo_create (surface);

        svg.render_document (cr, viewport);
        cairo_surface_write_to_png (surface, "example-out.png");

        cairo_destroy (cr);
        cairo_surface_destroy (surface);
        return 0;
    } catch (const rsvg::Error &e) {
        std::cerr << e.what () << '\n';
        return 1;
    }
}
