# Audit fixes (librsvg 2.63.5)

Ranked from `00-SUMMARY.md`. High first. Noise skipped.

## Order

1. **B3-5** — cap document / filter image surfaces (`RSVG_MAX_IMAGE_PIXEL_BYTES`)
2. **B3-1** — `gsize` pixel index math in filter walks
3. **H2-10** — cap feMorphology radius
4. **H2-11** — cap feTurbulence `numOctaves`
5. **E4-1 / SA-1 / H2-9** — volatile locals in PNG/JPEG `setjmp`
6. **H2-7** — GIF first-frame size check before RasterBits
7. **H2-8** — AVIF size limits before decode
8. **H2-1** — file size cap before full read
9. **E4-2** — XInclude parse failure uses xi:fallback
10. **E4-9** — remember push/pop offscreen layer instead of re-acquiring clip-path
11. **H2-16 leftover** — cap non-filter offscreen `create_similar`
12. **E4-8** — check gdk_pixbuf_new NULL in surface_to_pixbuf
13. **H2-4** — do not set XML_PARSE_HUGE unless UNLIMITED on libxml2 2.12+
14. **H2-12** — skip non-finite feDisplacementMap scale before the interpolator cast
15. **B3-2** — clamp feOffset dx/dy to gint and subtract in gint64
16. **B3-3** — skip kernelUnitLength scaling that would overflow the int canvas size
17. **E4-4** — failed write closes the loader so close cannot set CLOSED_OK

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

### H2-7 — GIF first-frame size check (try 1, committed)

- Site: `rsvg-image.c` `rsvg_cairo_surface_from_gif` called `DGifSlurp` before any per-frame size check, so a small GIF with a huge first frame (or many frames) allocated `RasterBits` unbounded.
- Plan: decode only frame 0; check `ImageDesc` with `rsvg_image_dims_ok` before `malloc`.
- Change: `rsvg_gif_load_first_frame` walks records, parses GCE for the transparent index, then `DGifGetImageDesc` + capped raster + `DGifGetLine` (with giflib’s interlace passes). Later frames are not read.
- Tests: `make check` 268 PASS including `/api/gif_huge_frame_does_not_abort` (8×8 screen, 40000×8 frame) and the existing red GIF tests. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

### H2-8 — AVIF size limits before decode (try 1, committed)

- Site: `rsvg-image.c` `rsvg_cairo_surface_from_avif` ran `avifDecoderNextImage` before `rsvg_image_dims_ok`, and never set libavif’s `imageSizeLimit` / `imageDimensionLimit`.
- Plan: set those limits to rsvg’s caps, then check container size after `avifDecoderParse` before the AV1 decode.
- Change: `imageDimensionLimit = RSVG_MAX_IMAGE_DIMENSION`; `imageSizeLimit = max_pixel_bytes / 4`. `rsvg_image_dims_ok` after parse and again after `NextImage`.
- Tests: `make check` 269 PASS including `/api/avif_oversized_dimensions_does_not_abort` (ispe 40000×10) and the existing 10×10 AVIF tests. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

### H2-1 — file size cap before full read (try 1, committed)

- Site: `rsvg-io.c` `rsvg_acquire_file_data` (`g_file_get_contents`) and `rsvg_acquire_gvfs_data` (`g_file_load_contents`). The `max_data_uri_bytes` argument only applied to `data:` URIs; files were read whole, then `<image>` checked length afterwards.
- Plan: thread `max_data_uri_bytes` into both acquire helpers; reject from `G_FILE_ATTRIBUTE_STANDARD_SIZE` before the full read; check the buffer length afterwards. Unknown size (query fail or size 0, including FIFOs) is allowed through, then the post-read check still applies.
- Change: `rsvg_gfile_within_size_limit` / `rsvg_buffer_within_size_limit`. Both file and GVFS paths cap so a huge local file is not re-read on the GVFS fallback. Main documents loaded via `rsvg_handle_new_from_file` also cap at `RSVG_MAX_DATA_URI_BYTES` (64 MiB). Stream acquire is unchanged (it does not slurp).
- Tests: `make check` 270 PASS including `/api/oversized_file_href_does_not_abort` (red PNG `ftruncate`d past the cap stays omitted; sibling under the cap still paints). `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**. FIFOs still report size 0 so they are not rejected before the read.

### E4-2 — XInclude parse failure uses fallback (try 1, committed)

- Site: `rsvg-base.c` `rsvg_start_xinclude`. `(void) xmlParseDocument` discarded the nested parse status; a failed `create_xml_stream_parser` cleared `err` and still set `success = TRUE`, so `xi:fallback` was skipped.
- Plan: treat parser-create failure, `xmlParseDocument != 0`, and stream `err` as include failure so fallback runs. Match the main-document path in `rsvg_handle_read_stream_sync`.
- Change: NULL parser → fallback. Nested parse result and `err` checked before `success = TRUE`. `ctx->priv->error` still forces fallback. Partial nodes from a half-parsed include can remain in the tree (no rollback).
- Tests: `make check` 271 PASS including `/api/xinclude_malformed_xml_uses_fallback` (not-xml sibling uses red fallback; well-formed sibling still paints blue and skips fallback). `make check-parity` pass=723 fail=0 skip=27 (include-fallback, include-compressed, include-text still green).
- Outcome: **fixed**.

### E4-9 — push/pop render stack clip-path layer mismatch (try 1, committed)

- Site: `rsvg-cairo-draw.c` `rsvg_cairo_push_render_stack` / `rsvg_cairo_pop_render_stack`. Push and pop each acquired the clip-path node to decide `lateclip` (objectBoundingBox). Pop's early return skipped unwinding `cr_stack` when the second acquire failed. `rsvg_cairo_render_free` was a TODO and did not drain leftover stacks.
- Plan: remember whether push created an offscreen layer; pop consults that flag instead of re-deriving skip from a second acquire. Drain leftover stacks in free.
- Change: `RsvgCairoRender.offscreen_stack` of `gboolean`. `push_render_stack` returns whether a layer was created. `pop_discrete_layer` only calls `pop_render_stack` when that flag is TRUE. `pop_render_stack` always unwinds (empty `cr_stack` returns before the lateclip acquire). `rsvg_cairo_render_free` drains `cr_stack`, bbox stacks, `surfaces_stack`, and `offscreen_stack`.
- Tests: `make check` 273 PASS including `/api/clip_path_object_bbox_still_clips` and `/api/clip_path_missing_does_not_unbalance_stack`. `make check-parity` pass=723 fail=0 skip=27 (masking-path-04-b still green). First `make check` SIGSEGV on masking-path-04-b was a stale `rsvg-cairo-clip.o` after inserting the struct field; rebuilt that TU, no source change.
- Outcome: **fixed**. Non-filter `cairo_surface_create_similar` still uses uncapped `render->width/height` (H2-16 remainder).

### H2-16 leftover — cap non-filter offscreen create_similar (try 1, committed)

- Site: `rsvg-cairo-draw.c` `rsvg_cairo_push_render_stack`. Filter surfaces were already dummy 1×1 when over the pixel budget (B3-5). Non-filter `cairo_surface_create_similar` still passed uncapped `render->width/height`. The `#if 0` status check would have returned early and unbalanced push/pop.
- Plan: share the dimension cap for both paths; on a failed cairo surface, replace with a 1×1 dummy instead of returning.
- Change: `rsvg_cairo_offscreen_size` clamps to 1×1 when non-finite, sub-pixel, over `RSVG_MAX_IMAGE_DIMENSION`, or over `max_image_pixel_bytes`. Both similar and image surfaces use it. A failed `cairo_surface_status` becomes a 1×1 image surface so the layer is still pushed.
- Tests: `make check` 275 PASS including `/api/huge_opacity_canvas_does_not_crash` (32000² identity-scale opacity group) and `/api/opacity_group_still_blends`. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**. Pattern-tile `create_similar` at rsvg-cairo-draw.c:292 is a different allocation and was left alone.

### E4-8 — gdk_pixbuf_new NULL in surface_to_pixbuf (try 1, committed)

- Site: `rsvg-cairo-draw.c` `rsvg_cairo_surface_to_pixbuf`. `gdk_pixbuf_new` can return NULL on overflow or OOM; the code then called `gdk_pixbuf_get_pixels (NULL)`.
- Plan: return NULL if the pixbuf allocation fails.
- Change: `if (dest == NULL) return NULL;` after `gdk_pixbuf_new`.
- Tests: `make check` 276 PASS including `/api/get_pixbuf_one_pixel_red` (public `rsvg_handle_get_pixbuf` still converts a 1×1 red surface). `make check-parity` pass=723 fail=0 skip=27. A first test draft called the internal symbol and failed to link (`G_GNUC_INTERNAL`); rewritten to use the public API. The OOM/overflow NULL path is not unit-tested (needs a multi-GiB surface after B3-5).
- Outcome: **fixed**.

### H2-4 — XML_PARSE_HUGE only with UNLIMITED (try 1, committed)

- Site: `rsvg-base.c` `rsvg_set_xml_parse_options`. The `#else` (libxml2 2.12+, `xmlParserMaxDepth` is `#define 256`) always ORed `XML_PARSE_HUGE`, which raises element depth 256→2048 and also drops the 10 MiB text-node, 50 KiB name, 100 MiB dict, and entity-nesting caps. Not taken on this host (libxml2 2.9.14). There is no `xmlCtxtSetMaxDepth`.
- Plan: never set HUGE unless `RSVG_HANDLE_FLAG_UNLIMITED`. On 2.12+ default documents stay at libxml's 256-element depth. Lower `css_deep_tree` to 240 so it stays portable. Older-libxml2 `#ifndef xmlParserMaxDepth` global raise to 384 is unchanged.
- Change: removed `options |= XML_PARSE_HUGE;` from the `#else`. UNLIMITED still ORs HUGE before the `#ifndef`. Tests: `css_deep_tree` depth `RSVG_MAX_CSS_TREE_DEPTH - 16`; new unlimited 320-deep load; new 60k-char name rejected without UNLIMITED.
- Tests: `make check` 278 PASS including `/api/css_deep_tree_does_not_abort`, `/api/css_deep_tree_unlimited_still_loads`, `/api/xml_huge_name_is_rejected_without_unlimited`. `make check-parity` pass=723 fail=0 skip=27. The 2.12+ `#else` is not compiled on this machine (2.9.14); the name-length test still covers HUGE-off caps here.
- Outcome: **fixed**.

### H2-12 — feDisplacementMap scale=NaN interpolator cast (try 1, committed)

- Site: `rsvg-filter.c` `rsvg_filter_primitive_displacement_map_set_atts` (`g_ascii_strtod` into `scale`) and `get_interp_pixel`. NaN comparisons are all false, so the bounds test fell through to `(gint) NaN` (UB). Lighting also calls `get_interp_pixel`. After B3-1 the index uses `(gint)` rather than `(guint)`; still UB.
- Plan: keep the default scale 0 when the attribute is not finite; return 0 from `get_interp_pixel` when `ox`/`oy` are not finite.
- Change: `if (isfinite (scale)) filter->scale = scale;` at parse. Early `return 0` in `get_interp_pixel` when `!isfinite (ox) || !isfinite (oy)`.
- Tests: `make check` 280 PASS including `/api/displacement_nan_scale_does_not_abort` (identity, interior red) and `/api/displacement_finite_scale_still_applies` (scale=16 on red, interior y=4 samples out of bounds). `make check-parity` pass=723 fail=0 skip=27 (filters-displace-02-f still green). First smoke used canvas-edge y=0, which `get_interp_pixel` already treats as out of bounds (`foy <= y0`); assertions moved to y=4.
- Outcome: **fixed**.

### B3-2 — feOffset dx/dy int conversion (try 1, committed)

- Site: `rsvg-filter.c` `rsvg_filter_primitive_offset_render`. Affine product assigned to `int ox, oy` (float-cast UB for NaN/Inf/out-of-range). Bounds test `x - ox` is signed-overflow UB when `ox` is `G_MININT`.
- Plan: clamp the device-pixel offset to a finite gint; do the source-index subtract in `gint64` so a huge offset is simply out of bounds.
- Change: `rsvg_offset_to_gint` (non-finite → 0, else clamp to `G_MININT`..`G_MAXINT`). Loop uses `gint64 sx/sy`; after the bounds check they fit in `gint` for `rsvg_pixel_offset`.
- Tests: `make check` 282 PASS including `/api/offset_huge_dx_does_not_abort` (`dx='1e20'` copies nothing) and `/api/offset_unit_dx_still_shifts`. `make check-parity` pass=723 fail=0 skip=27 (filter-offset and filters-offset-01-b still green).
- Outcome: **fixed**.

### B3-3 — kernelUnitLength ceil overflow (try 1, committed)

- Site: `rsvg-filter.c` `rsvg_filter_primitive_convolve_matrix_render` and `feDiffuseLighting`. `ox`/`oy` below `1e-6` are already bumped to 1. Tiny but still-positive `ox` in `[1e-6, width/INT_MAX)` makes `width/ox` exceed `INT_MAX`, so `(int) ceil` is float-cast UB, and an in-range value can still request a huge `surface_scale_clipped`.
- Plan: compute the scaled size in double, reject non-finite/non-positive `ox`/`oy`, sizes over `RSVG_MAX_IMAGE_DIMENSION` or the UNLIMITED pixel budget, and bounds that do not fit in `gint`. On reject, keep the unscaled surface (same as a failed scale).
- Change: `rsvg_kernel_unit_scaled_size` before `surface_scale_clipped`. Both call sites use it. Specular lighting has no third copy.
- Tests: `make check` 284 PASS including `/api/convolve_tiny_kernel_unit_does_not_abort` (2200×8 identity 3×3, `kernelUnitLength='1e-6'`) and `/api/convolve_unit_kernel_length_still_applies`. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

### E4-4 — failed write then close CLOSED_OK (try 1, committed)

- Site: `rsvg-base.c` `rsvg_handle_write_impl`. A SAX callback can set `real_error` while `xmlParseChunk` returns 0 (self `xi:include` nesting). Write returned FALSE, left the handle in READING, and cleared `priv->error`. Close then used a fresh stack error; the finishing chunk succeeded and set `CLOSED_OK`, so render/get_pixbuf still worked. Docs for `rsvg_handle_write` say a FALSE result closes the loader. `gdk-pixbuf-loader/io-svg.c` still calls close after a failed write.
- Plan: on any write_impl failure, free the parser and set `CLOSED_ERROR`. Close stays idempotent TRUE; render stays blocked.
- Change: both `xmlParseChunk != 0` and SAX `real_error` paths set `CLOSED_ERROR`, free `ctxt`, and clear `priv->error` (avoids the dangling pointer on the xml-error path). Loader `io-svg.c` unchanged.
- Tests: `make check` 286 PASS including `/api/failed_write_then_close_does_not_succeed` (self xi:include via write, close, get_pixbuf NULL) and `/api/write_then_close_still_renders`. `make check-parity` pass=723 fail=0 skip=27.
- Outcome: **fixed**.

## Remaining (not this batch)

- Medium: none.
- Low / note: see `00-SUMMARY.md`.
