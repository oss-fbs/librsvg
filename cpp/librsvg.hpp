/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
   librsvg.hpp: Optional C++ wrapper around the librsvg-2.0 C API.

   Header-only. Not built or installed by make / make install.
   Does not change the C library, ABI, or soname.

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

#ifndef RSVG_CXX_LIBRSVG_HPP
#define RSVG_CXX_LIBRSVG_HPP

#include <librsvg/rsvg.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace rsvg {

/**
 * GError turned into a C++ exception. Copies the message, then
 * frees the GError. Does not abort.
 */
class Error : public std::runtime_error {
public:
    explicit Error (const char *fallback)
        : std::runtime_error (fallback ? fallback : "librsvg error"),
          domain_ (0),
          code_ (0)
    {
    }

    /* Takes ownership of err (may be NULL). */
    explicit Error (GError *err)
        : std::runtime_error (err && err->message ? err->message : "librsvg error"),
          domain_ (err ? err->domain : 0),
          code_ (err ? err->code : 0)
    {
        if (err)
            g_error_free (err);
    }

    GQuark domain () const
    {
        return domain_;
    }

    int code () const
    {
        return code_;
    }

private:
    GQuark domain_;
    int code_;
};

/** Ink and logical boxes from get_geometry_for_*(). */
struct Geometry {
    RsvgRectangle ink;
    RsvgRectangle logical;
};

/** Width / height / viewBox as stored on the SVG root. */
struct IntrinsicDimensions {
    bool has_width;
    RsvgLength width;
    bool has_height;
    RsvgLength height;
    bool has_viewbox;
    RsvgRectangle viewbox;
};

/**
 * Move-only owner of an RsvgHandle. Destructor calls g_object_unref.
 * Callers never see the raw handle pointer.
 */
class Handle {
public:
    /* Empty handle. Set DPI / base URI first, then read_stream(). */
    explicit Handle (RsvgHandleFlags flags = RSVG_HANDLE_FLAGS_NONE)
        : handle_ (rsvg_handle_new_with_flags (flags))
    {
        if (!handle_)
            throw Error ("rsvg_handle_new_with_flags failed");
    }

    Handle (Handle &&other) noexcept
        : handle_ (other.handle_)
    {
        other.handle_ = NULL;
    }

    Handle &operator= (Handle &&other) noexcept
    {
        if (this != &other) {
            reset ();
            handle_ = other.handle_;
            other.handle_ = NULL;
        }
        return *this;
    }

    ~Handle ()
    {
        reset ();
    }

    Handle (const Handle &) = delete;
    Handle &operator= (const Handle &) = delete;

    /* --- loaders: each returns an owned handle or throws Error --- */

    static Handle from_file (const char *filename)
    {
        GError *err = NULL;
        RsvgHandle *h = rsvg_handle_new_from_file (filename, &err);
        return Handle (take (h, err, "load from file failed"));
    }

    static Handle from_file (const std::string &filename)
    {
        return from_file (filename.c_str ());
    }

    static Handle from_data (const guint8 *data, gsize data_len)
    {
        GError *err = NULL;
        RsvgHandle *h = rsvg_handle_new_from_data (data, data_len, &err);
        return Handle (take (h, err, "load from memory failed"));
    }

    static Handle from_data (const void *data, std::size_t data_len)
    {
        return from_data (static_cast<const guint8 *> (data),
                          static_cast<gsize> (data_len));
    }

    static Handle from_data (const std::string &svg)
    {
        return from_data (reinterpret_cast<const guint8 *> (svg.data ()),
                          static_cast<gsize> (svg.size ()));
    }

    /* file is caller-owned. flags default to none; cancellable may be NULL. */
    static Handle from_gfile (GFile *file,
                              RsvgHandleFlags flags = RSVG_HANDLE_FLAGS_NONE,
                              GCancellable *cancellable = NULL)
    {
        GError *err = NULL;
        RsvgHandle *h = rsvg_handle_new_from_gfile_sync (file, flags,
                                                         cancellable, &err);
        return Handle (take (h, err, "load from GFile failed"));
    }

    static Handle from_stream (GInputStream *stream,
                               GFile *base_file = NULL,
                               RsvgHandleFlags flags = RSVG_HANDLE_FLAGS_NONE,
                               GCancellable *cancellable = NULL)
    {
        GError *err = NULL;
        RsvgHandle *h = rsvg_handle_new_from_stream_sync (stream, base_file,
                                                          flags, cancellable,
                                                          &err);
        return Handle (take (h, err, "load from stream failed"));
    }

    /* Load into an empty handle. Call after set_base_uri / set_dpi. */
    void read_stream (GInputStream *stream, GCancellable *cancellable = NULL)
    {
        GError *err = NULL;
        gboolean ok = rsvg_handle_read_stream_sync (handle_, stream,
                                                    cancellable, &err);
        check (ok, err, "read_stream failed");
    }

    /* --- setup (void C APIs; no GError) --- */

    void set_dpi (double dpi)
    {
        rsvg_handle_set_dpi (handle_, dpi);
    }

    void set_dpi (double dpi_x, double dpi_y)
    {
        rsvg_handle_set_dpi_x_y (handle_, dpi_x, dpi_y);
    }

    /* Must run before read_stream() on an empty handle. */
    void set_base_uri (const char *base_uri)
    {
        rsvg_handle_set_base_uri (handle_, base_uri);
    }

    void set_base_gfile (GFile *base_file)
    {
        rsvg_handle_set_base_gfile (handle_, base_file);
    }

    /* css_len is required; the C API does not use strlen. */
    void set_stylesheet (const guint8 *css, gsize css_len)
    {
        GError *err = NULL;
        gboolean ok = rsvg_handle_set_stylesheet (handle_, css, css_len, &err);
        check (ok, err, "set_stylesheet failed");
    }

    void set_stylesheet (const char *css, std::size_t css_len)
    {
        set_stylesheet (reinterpret_cast<const guint8 *> (css),
                        static_cast<gsize> (css_len));
    }

    void set_stylesheet (const std::string &css)
    {
        set_stylesheet (css.data (), css.size ());
    }

    /* NULL clears. Cancel from another thread with g_cancellable_cancel(). */
    void set_cancellable_for_rendering (GCancellable *cancellable)
    {
        rsvg_handle_set_cancellable_for_rendering (handle_, cancellable);
    }

    /* --- render: viewport is Cairo user units --- */

    void render_document (cairo_t *cr, const RsvgRectangle &viewport)
    {
        GError *err = NULL;
        gboolean ok = rsvg_handle_render_document (handle_, cr, &viewport, &err);
        check (ok, err, "render_document failed");
    }

    /* id is "#foo", or NULL for the whole SVG. Fitted to the document viewport. */
    void render_layer (cairo_t *cr, const char *id, const RsvgRectangle &viewport)
    {
        GError *err = NULL;
        gboolean ok = rsvg_handle_render_layer (handle_, cr, id, &viewport, &err);
        check (ok, err, "render_layer failed");
    }

    /* id is "#foo". element_viewport is that element's own box. */
    void render_element (cairo_t *cr, const char *id,
                         const RsvgRectangle &element_viewport)
    {
        GError *err = NULL;
        gboolean ok = rsvg_handle_render_element (handle_, cr, id,
                                                  &element_viewport, &err);
        check (ok, err, "render_element failed");
    }

    /* --- geometry --- */

    Geometry geometry_for_layer (const char *id, const RsvgRectangle &viewport) const
    {
        Geometry g = {};
        GError *err = NULL;
        gboolean ok = rsvg_handle_get_geometry_for_layer (handle_, id, &viewport,
                                                          &g.ink, &g.logical,
                                                          &err);
        check (ok, err, "geometry_for_layer failed");
        return g;
    }

    Geometry geometry_for_element (const char *id) const
    {
        Geometry g = {};
        GError *err = NULL;
        gboolean ok = rsvg_handle_get_geometry_for_element (handle_, id,
                                                            &g.ink, &g.logical,
                                                            &err);
        check (ok, err, "geometry_for_element failed");
        return g;
    }

    /* false if width/height cannot be resolved to pixels. */
    bool intrinsic_size_in_pixels (double &width, double &height) const
    {
        gdouble w = 0.0, h = 0.0;
        gboolean ok = rsvg_handle_get_intrinsic_size_in_pixels (handle_, &w, &h);
        if (ok) {
            width = w;
            height = h;
        }
        return ok != FALSE;
    }

    IntrinsicDimensions intrinsic_dimensions () const
    {
        IntrinsicDimensions d = {};
        gboolean has_w = FALSE, has_h = FALSE, has_vb = FALSE;
        rsvg_handle_get_intrinsic_dimensions (handle_,
                                              &has_w, &d.width,
                                              &has_h, &d.height,
                                              &has_vb, &d.viewbox);
        d.has_width = has_w != FALSE;
        d.has_height = has_h != FALSE;
        d.has_viewbox = has_vb != FALSE;
        return d;
    }

    bool has_sub (const char *id) const
    {
        return rsvg_handle_has_sub (handle_, id) != FALSE;
    }

private:
    explicit Handle (RsvgHandle *owned)
        : handle_ (owned)
    {
    }

    void reset () noexcept
    {
        if (handle_) {
            g_object_unref (handle_);
            handle_ = NULL;
        }
    }

    static RsvgHandle *take (RsvgHandle *h, GError *err, const char *fallback)
    {
        if (h)
            return h;
        if (err)
            throw Error (err);
        throw Error (fallback);
    }

    static void check (gboolean ok, GError *err, const char *fallback)
    {
        if (ok)
            return;
        if (err)
            throw Error (err);
        throw Error (fallback);
    }

    RsvgHandle *handle_;
};

} /* namespace rsvg */

#endif /* RSVG_CXX_LIBRSVG_HPP */
