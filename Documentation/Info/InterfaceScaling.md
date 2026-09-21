# Text Scale & Interface Scale

If the editor is hard to read on your display (a Retina MacBook, a 4K monitor, a TV across the
room), there are two settings that make it bigger. Both live in
`Edit → Preferences → Appearance`.

| Setting | What grows | Sharpness | Use it when |
|---|---|---|---|
| **Text Scale** | Text and icons only. Panels and spacing mostly stay the same size. | Always sharp: fonts are re-drawn at the new size. | The layout is fine but the **text is too small**. |
| **Interface Scale** | Everything: text, icons, buttons, padding, panels. | Magnified like a zoomed image, so it gets soft at large values. | **Everything is too small**, typically on a HiDPI display. |

They can be combined. A good recipe for a HiDPI screen is to set Interface Scale so buttons and
panels are a comfortable size, then nudge Text Scale up a little if you still want larger text.

## Changing Text Scale

1. `Edit → Preferences → Appearance`.
2. Drag the **Text Scale** slider (0.75 – 2.0), or `Ctrl + Click` it to type an exact value.
3. Press **Apply**. The editor rebuilds its fonts, which takes a fraction of a second.
4. **Reset** returns to `1.00`.

Nothing changes until you press Apply, so you can drag the slider around without the interface
jumping under your mouse. The value is saved as soon as you apply it; you do not need the
Preferences window's own Apply / OK buttons for this setting, and Cancel will not revert it.

Text Scale affects the whole editor, including the Terminal panel and the built-in script editor.

### Known limits

- A few controls have fixed pixel widths. At large scales some labels can be clipped, for example
  in narrow dropdowns. The main toolbar and the Preferences window already grow with the text
  scale; if you find a control that does not, please report it.
- Text drawn **inside the viewport** by the game itself (stats overlay, on-screen log) belongs to
  the running scene, not the editor UI, and is not affected.

## Changing Interface Scale

1. `Edit → Preferences → Appearance`, or `View → Interface Scale` for the quick version.
2. Drag the slider (0.5 – 3.0), or `Ctrl + Click` to type an exact value such as `1.25`.
3. Press **Apply**.
4. **Reset** returns to the default for your display: `1.00`, or the display's scale factor
   (usually `2.00`) on a Retina Mac.

The slider shows two decimals and `Ctrl + Click` lets you type, so landing on exactly `1.00` no
longer depends on a steady hand. If the interface ever ends up unusably large or small, **Reset**
in either location fixes it in one click.

Interface Scale is stored in the project's `Config.ini` (`EditorInterfaceScale=`), so it travels
with the project. With no project open the new scale still takes effect, but only until you close
the editor, because there is no `Config.ini` to write it to. Text Scale is stored with your
personal editor preferences and applies to every project you open.

### macOS / Retina

On a Retina display the editor starts with Interface Scale set to the display's scale factor
(2.0 on most MacBooks), because the engine works in physical pixels and the UI would otherwise be
half size. If that default still reads small, raise **Text Scale** to `1.15` – `1.3` rather than
pushing Interface Scale higher: text stays crisp and you keep more room for panels.

## Where the settings are stored

| Setting | File |
|---|---|
| Text Scale | `Appearance.json` (`textScale`) in the editor preferences folder |
| Interface Scale | `Config.ini` (`EditorInterfaceScale`) in the project folder |

Editor preferences folder:

- **Windows:** `%APPDATA%/PolyphaseEditor/Preferences/`
- **macOS:** `~/Library/Application Support/PolyphaseEditor/Preferences/`
- **Linux:** `~/.config/PolyphaseEditor/Preferences/`

If a bad value ever stops the editor from being usable, close it and delete the `textScale` line
(or the `EditorInterfaceScale` line) from the file above; the default is used on the next start.

---

Related: [Themes / CSS](../Development/ThemesCss.md) for colours and the editor font.
Contributors: see [Editor Text Scaling internals](../Development/EditorTextScaling.md).
