# Watch fonts

The watch bundles a **subsetted Fredoka** for the hero glyphs (big count, timers,
position, NEXT UP name); small 9–11px labels use the built-in system Gothic.

## To add Fredoka

1. Download **Fredoka** (Google Fonts, OFL) and drop the `.ttf` here, e.g.
   `Fredoka-Medium.ttf`.
2. Register each needed point size in `package.json` under `pebble.resources.media`.
   Use `characterRegex` to keep the bundle inside the 256 KB resource budget — we
   only need digits, separators, and uppercase letters:

   ```json
   {
     "type": "font",
     "name": "FONT_FREDOKA_34",
     "file": "fonts/Fredoka-Medium.ttf",
     "characterRegex": "[0-9:/A-Z ]"
   }
   ```

   Repeat for the ~25px and ~15px sizes (the resource `name` must end in the point
   size; the compiler emits `RESOURCE_ID_FONT_FREDOKA_34`).
3. Load in C with `fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FREDOKA_34))`
   and unload on deinit.

Until the `.ttf` is added, `media` stays empty so the project builds.
