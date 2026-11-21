# Test fonts (data only)

Roboto, Ahem, DejaVu Sans, and Noto Sans Hebrew are the 2.62.3
reftest fonts (copied during development). They are font files, not
Rust code. `fonts.conf` maps `sans` / `sans-serif` → Roboto. Cache is
`/tmp/rsvg_c_tests_fontconfig_cache`.

`RSVG_PARITY=1` (`make check-parity`) loads **only** the 2.62.3
TTF list (Roboto / Ahem / DejaVu / Noto Hebrew) plus `fonts.conf`.
It does **not** `AddDir` this folder, so LiberationSans stays out of
the isolated map. `make check` still uses only LiberationSans.

`LiberationSans-Regular.ttf` is also the fallback if the directory
cannot be loaded.

Do not install these onto the host. Do not run `fc-cache`.
