# librsvg {#mainpage}

**librsvg** is a C library that loads SVG and SVGZ and draws it onto
a Cairo surface.  The shared library, headers, and pkg-config module
are **librsvg-2.0**.  This is the public C API for librsvg **2.63.5**.

It handles non-animated SVG, CSS styling, filters, masks, clip paths,
and SVG `<image>` rasters (PNG, JPEG, GIF, WebP, BMP; optional AVIF).  A
gdk-pixbuf loader and `rsvg-convert` are included.  See the
[examples](@ref examples).

## Limits

`--unlimited` / `RSVG_HANDLE_FLAG_UNLIMITED` raises decoded pixels to
512 MiB and `data:` URI size to 256 MiB. Side length, file nesting,
layer nesting, and Cairo coordinates stay at their default values.

File nesting (XInclude, cross-document references, CSS `@import`) is
limited to 20.  Exceeding it is a load error.

Layer nesting during drawing is limited to 500 discrete layers on
glibc and 50 on musl.  Path and text coordinates outside Cairo's 24.8
fixed-point range are skipped.

SVG `<image>` rasters: maximum side 32767 pixels (never raised).
Decoded pixels 128 MiB default, 512 MiB with `--unlimited`. `data:`
URI size 64 MiB default, 256 MiB with `--unlimited`. Very large
stylesheet buffers may be rejected.

## API groups

- [Types](@ref rsvg_types) — #RsvgHandle, #RsvgRectangle, #RsvgLength, #RsvgUnit
- [Loading](@ref rsvg_loading) — `new_from_file`, `new_from_data`, GIO
- [Rendering](@ref rsvg_rendering) — `render_document`, `render_layer`, `render_element`
- [Geometry](@ref rsvg_geometry) — `get_geometry_for_*`, `get_intrinsic_*`
- [Stylesheet](@ref rsvg_stylesheet) — `set_stylesheet`
- [Pixbuf](@ref rsvg_pixbuf) — `get_pixbuf_and_error`
- [Cancel](@ref rsvg_cancel) — `set_cancellable_for_rendering`
- [Version](@ref rsvg_version) — `LIBRSVG_CHECK_VERSION`, `LIBRSVG_VERSION`
- [Deprecated](@ref rsvg_deprecated) — older calls kept for compatibility

The last new public C function is `rsvg_handle_set_cancellable_for_rendering`
(2.59).  The library version is 2.63.5.

## C++ {#cpp}

Optional header-only wrapper in `cpp/` (`librsvg.hpp`).  It is not
built or installed by `make`.  Link the C library as usual.

### API

`rsvg::Handle` is a move-only owner of an `RsvgHandle`.  The destructor
calls `g_object_unref`.  Failures throw `rsvg::Error` (`std::runtime_error`).

- Load: `from_file`, `from_data`, `from_gfile`, `from_stream`, `read_stream`
- Setup: `set_dpi`, `set_base_uri`, `set_base_gfile`, `set_stylesheet`, `set_cancellable_for_rendering`
- Render: `render_document`, `render_layer`, `render_element`
- Geometry: `geometry_for_layer`, `geometry_for_element`, `intrinsic_size_in_pixels`, `intrinsic_dimensions`, `has_sub`

### Example

From `cpp/`:

    c++ -std=c++11 -I. example.cpp \
        $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)

Run from `cpp/` so `../examples/icon.svg` is found.  The sample loads
that file, sets 96 DPI, queries pixel size, calls `render_document`,
and writes `example-out.png`.  Source: `cpp/example.cpp`.

@include example.cpp

[Thank you](@ref thanks) — the people who built librsvg.

## Licence {#licence}

The **library** (`librsvg-2.so`) is licensed under the GNU Library
General Public License, version 2 or later.  You may link this library
from other programs; the library itself stays under this license.
The full text is <a href="LICENCE-LIB">LICENCE-LIB</a>.

The command-line tools (`rsvg-convert` and similar) are licensed under
the GNU General Public License, version 2.  The full text is
<a href="LICENCE">LICENCE</a>.

## Building this HTML

Doxygen is required.  From the project root:

    doxygen doc/Doxyfile

or `make html`.  Either command writes `doc/html/index.html`.
