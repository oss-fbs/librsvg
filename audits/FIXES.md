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
