# Audit fixes (librsvg 2.63.5)

Ranked from `00-SUMMARY.md`. High first. Noise skipped.

## Order

1. **B3-5** — cap document / filter image surfaces (`RSVG_MAX_IMAGE_PIXEL_BYTES`)
2. **B3-1** — `gsize` pixel index math in filter walks
3. **H2-10** — cap feMorphology radius
4. **H2-11** — cap feTurbulence `numOctaves`
5. Medium / low left for later batches

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
