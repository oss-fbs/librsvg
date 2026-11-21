# Audit fixes (librsvg 2.63.5)

Ranked from `00-SUMMARY.md`. High first. Noise skipped.

## Order

1. **B3-5** — cap document / filter image surfaces (`RSVG_MAX_IMAGE_PIXEL_BYTES`)
2. **B3-1** — `gsize` pixel index math in filter walks
3. **H2-10** — cap feMorphology radius
4. **H2-11** — cap feTurbulence `numOctaves`
5. **E4-1 / SA-1 / H2-9** — volatile locals in PNG/JPEG `setjmp`
6. Medium left: H2-7 / H2-8 / H2-1, then E4-2 / E4-9 / H2-16 / E4-8, then H2-4 / H2-12 / B3-2 / B3-3 / E4-4

## Attempts

### B3-5 — canvas pixel cap (try 1, committed)

- Sites: `rsvg.c` pixbuf, `rsvg-cairo-draw.c` filter offscreen, `rsvg-filter.c` `_rsvg_image_surface_new`.
- Plan: reuse the raster byte cap; 1×1 dummy if a filter canvas is oversize so push/pop stay balanced.
- Change: `rsvg_pixel_buffer_dims_ok` in `rsvg-private.h`. Drawing ctx copies `max_image_pixel_bytes` from the handle. `_rsvg_image_surface_new` hard-caps at UNLIMITED (512 MiB). `rsvg_handle_get_pixbuf_sub` returns NULL when the document exceeds the handle cap. Filter offscreen in `rsvg_cairo_push_render_stack` falls back to a 1×1 dummy so push/pop stay balanced (E4-9).
- Tests: `make check` 261 PASS including `/api/huge_filtered_canvas_does_not_crash`. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**. Non-filter `cairo_surface_create_similar` still uses uncapped `render->width/height` (H2-16 remainder).

### B3-1 — filter pixel index math (try 1, committed)

- Site: `rsvg-filter.c` `y * rowstride` (~57 ARGB32 walks); reproduced at feOffset.
- Plan: `rsvg_pixel_offset` / `rsvg_row_offset` in `gsize`; replace index expressions.
- Change: helpers in `rsvg-private.h`. All ARGB32 walks in `rsvg-filter.c` use them, plus blur row/column pointer math (`rsvg_row_offset`). `int p` pixel indexes became `gsize`.
- Tests: `make check` 262 PASS including `/api/filter_pixel_offset_uses_gsize` (30000×128000 overflows int). `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

### H2-10 — feMorphology radius cap (try 1, committed)

- Site: `rsvg_filter_primitive_erode_render` `kx = rx * paffine.xx` with no cap.
- Plan: clamp scaled device-pixel radius at `RSVG_MAX_MORPHOLOGY_RADIUS` (500); non-finite or ≤0 → 0.
- Change: `RSVG_MAX_MORPHOLOGY_RADIUS` in `rsvg-limits.h`. `rsvg_morphology_kernel_radius` converts `rx * affine` to a finite gint in 0..500.
- Tests: `make check` 264 PASS including `/api/morphology_huge_radius_does_not_hang` and `/api/morphology_unit_radius_still_dilates`. `make check-parity` pass=723 fail=0 skip=27 (filters-morph-01-f still green).
- Outcome: **fixed**.

### H2-11 — feTurbulence numOctaves cap (try 1, committed)

- Site: `atoi` into `nNumOctaves`; loop in `feTurbulence_turbulence`.
- Plan: clamp to 0..`RSVG_MAX_TURBULENCE_OCTAVES` (8) at parse and in the octave loop.
- Change: `RSVG_MAX_TURBULENCE_OCTAVES` in `rsvg-limits.h`. Parse uses `g_ascii_strtoll` then clamps. The octave loop clamps again.
- Tests: `make check` 266 PASS including `/api/turbulence_huge_octaves_does_not_hang` and `/api/turbulence_one_octave_still_applies`. `make check-parity` pass=723 fail=0 skip=27 (filters-turb-01-f and filters-turb-02-f still green).
- Outcome: **fixed**.

### E4-1 / SA-1 / H2-9 — PNG/JPEG setjmp volatiles (try 1, committed)

- Sites: `rsvg-image.c` `rsvg_cairo_surface_from_png` (`rgba`/`rows`) and `rsvg_cairo_surface_from_jpeg` (`rgba`/`created`).
- Plan: mark locals that are assigned after `setjmp` and read after `longjmp` as `volatile` (C11 7.13.2.1).
- Change: PNG `rgba`/`rows` are `volatile`. JPEG `rgba`/`created` are `volatile`; after `jpeg_destroy_decompress` the pixels move to a non-volatile pointer for EXIF/cairo. Truncated-IDAT PNG test hits the fail path after the decode buffer is allocated. A truncated-SOS JPEG test was dropped: libjpeg accepts that payload and paints, so it does not longjmp.
- Tests: `make check` 267 PASS including `/api/png_truncated_idat_does_not_abort`. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

## Remaining (not this batch)

- Medium: H2-7 / H2-8 / H2-1 (decoder/file size before decode), E4-2 / E4-9 / H2-16 / E4-8 (error-path robustness). H2-16 remainder: non-filter `cairo_surface_create_similar` still uses uncapped `render->width/height`.
- Also medium: H2-4, H2-12, B3-2, B3-3, E4-4.
- Low / note: see `00-SUMMARY.md`.
