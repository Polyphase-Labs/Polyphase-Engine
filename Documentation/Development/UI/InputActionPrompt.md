# Input Action Prompts

The `InputActionPrompt` widget displays the currently bound input for a named action — a sprite, a font glyph, or a text fallback — and swaps presentation automatically when the player switches device (keyboard ↔ controller, Xbox ↔ DualSense, etc).

It is the cross-platform companion to the [PlayerInput](../PlayerInput.md) action system. Where `PlayerInput` answers "is `Game.Interact` active?", `InputActionPrompt` answers "what should I draw on screen to tell the player how to *trigger* `Game.Interact` right now?"

---

## What you'll set up

| Piece | Type | Owns |
|---|---|---|
| `InputPromptMap` | Asset | the per-(platform, device, input) → sprite/glyph/text rows |
| `InputPromptStyle` | Asset | icon size, tint, spacing, fallback priority |
| `InputActionPrompt` | Widget | renders the resolved prompt in the UI |
| `InputActions.oct` | Existing asset | the action → binding map you already author for PlayerInput |

The widget is stateless — it reads the action's *current* binding every frame from `PlayerInput`, looks the binding's *path* up in the `InputPromptMap`, and renders the entry through `InputPromptStyle`. You don't tell the widget "show the gamepad icon" — you tell it "show whatever's bound to `Game.Interact`" and it figures the rest out.

---

## Quick start (artist flow)

### 1. Create an `InputPromptMap` asset

In the Content Browser, right-click → **New Asset → InputPromptMap**. Name it `IPM_Gameplay` or similar. Double-click to open it in the Inspector.

The inspector replaces the default property table with an entry editor:

```
[+ Add Entry]  [Duplicate]  [Delete]  [Up] [Down]    Test Device: [ Auto ▾ ]
[Platform: All ▾]  [Gamepad: All ▾]  [Path: ___________________]

# | Platform | Device     | Path             | Kind   | Asset/Glyph     | Fallback | X
--+----------+------------+------------------+--------+-----------------+----------+--
0 | Any      | Any/N-A    | Keyboard/F       | Text   | (text fallback) | F        | X
1 | Any      | Standard   | Gamepad.Button/A | Sprite | T_XboxA  [img]  | A        | X
2 | Any      | DualSense  | Gamepad.Button/A | Sprite | T_PSCross[img]  | Cross    | X
```

For each entry:

- **+ Add Entry** appends a blank row. Select it.
- Click **[Capture]** next to the Path field. The capture modal opens — press the key, mouse button, or gamepad input you want this row to cover. The Path is filled in canonical form (`Keyboard/F`, `Gamepad.Button/A`, `Gamepad.Axis/LTrigger+`). The Device column is auto-filled when the capture was a gamepad.
- Pick a **Kind** — Sprite, Glyph, or Text.
- For **Sprite**: drag a `Texture` asset from the Content Browser onto the Asset cell.
- For **Glyph**: drag a `Font` asset, then type the codepoint as hex (e.g. `E021`).
- For **Text**: just type the label (`F`, `LMB`, `A`).
- **Fallback** is always editable — it's used both for the `Text` kind directly and as the last resort if a sprite/glyph fails to load.

The Inspector's validation strip at the bottom lists rows with missing assets or empty paths. Click **[Jump]** to scroll to the offender.

### 2. Create an `InputPromptStyle` asset

Right-click → **New Asset → InputPromptStyle**. Default values:

| Field | Default |
|---|---|
| Icon Size | 24 px |
| Spacing | 2 px |
| Tint | white |
| Priority | Sprite → Glyph → Text |

Click the priority chips to swap their order — useful when you want text first for an accessibility-focused style, or glyph-only for a retro feel.

The **Prewarm Actions** list lets you list `Category/Name` pairs (e.g. `Game/Interact`, `Game/Reload`) that should be resolved on asset load instead of lazily on first widget render. Use it for hot-path HUD prompts that show up the instant gameplay starts.

The **Live Preview** at the bottom shows the resolved label for the same action across three devices (Keyboard, Xbox, DualSense) — useful for sanity-checking changes without touching a scene.

### 3. Drop an `InputActionPrompt` in your UI

In a Scene's widget tree, **Add Node → Widget → InputActionPrompt**. Set:

- **Action Category** — usually `Game`, `UI`, etc. — matches your `InputActionsAsset`.
- **Action Name** — e.g. `Interact`.
- **Prompt Map** — drag your `IPM_Gameplay`.
- **Prompt Style** — drag your style.
- **Auto Size** — leave on; the widget grows to the icon/text size.

That's it. The widget now displays the current binding for `Game.Interact` and refreshes within one frame when the player switches device.

---

## Rich-text tag in the `Text` widget

You can also embed action prompts inline in any `Text` widget by writing `[action:Category.Name]` in the text string:

```
Press [action:Game.Interact] to open the door.
Hold [action:Game.Sprint] to run.
```

At render time the tag is replaced with the resolved label (`F`, `Gamepad.A`, `LMB`, etc.). This is the cheap path — it does **not** render inline sprites or glyphs, just the text label. For full icon rendering inline, use `InputActionPrompt` widgets next to your text and lay them out with a horizontal container.

Malformed tags (`[action:foo` with no closing `]`) render verbatim. Unknown actions fall back to the action name. The substitution is recomputed every frame the Text widget is dirty, so device swaps refresh the text automatically.

---

## Lua API

```lua
function HUD:Start()
    self.prompt = self:CreateChild("InputActionPrompt")
    self.prompt:SetAction("Game", "Interact")
    self.prompt:SetPromptMap(LoadAsset("IPM_Gameplay"))
    self.prompt:SetPromptStyle(LoadAsset("IPS_HUD"))
end

function HUD:OnDoorEnter()
    -- Mirror the displayed prompt as a tooltip / log line:
    Log.Debug("Press " .. self.prompt:GetResolvedLabel() .. " to open")
end
```

| Function | Purpose |
|---|---|
| `prompt:SetAction(category, name)` | Change which action is displayed |
| `prompt:GetActionCategory()` / `GetActionName()` | Read back the current action id |
| `prompt:SetPromptMap(map)` / `SetPromptStyle(style)` | Hot-swap the assets |
| `prompt:GetResolvedLabel()` | Get the last-rendered text label (for tooltips, accessibility) |

---

## Authoring conventions

These aren't enforced — they're the patterns the system was designed around.

**One map per gameplay context, not one map per controller.**
Add Windows-Keyboard, Standard-gamepad, DualSense-gamepad rows all inside `IPM_Gameplay`. The resolver picks the right row per device.

**Use `Platform: Any` unless you have to.**
A Keyboard/F row almost never needs to differ between Windows and Linux. Only set a Platform when the input itself is platform-specific (e.g. a Touch row that only makes sense on Android).

**Always populate Fallback text.**
The Fallback field doubles as the "what to read out loud" string for screen-reader / accessibility scenarios, and as the safety net when a sprite asset fails to load.

**Glyph kind needs a font built for it.**
The widget passes the codepoint through the Font asset's normal glyph atlas. To render Kenney-style controller glyphs from a `.ttf`, either author a Font that maps the PUA codepoints into the ASCII range (the standard prompt-font hack), or stick to Sprite kind and assign individual textures.

---

## Verifying it works

1. Place an `InputActionPrompt` in a scene's widget tree.
2. Assign action `Game.Interact`, your map, and your style.
3. In the editor, tap a key — the widget shows `F` (or whatever you bound).
4. Wiggle a gamepad stick or press a button — the widget swaps to the gamepad sprite within one frame.
5. Tap a key again — it swaps back.

If it doesn't swap: verify the `InputActions.oct` has both keyboard *and* gamepad bindings for the action (one binding per device). The widget can only display what's bound.

If a row never matches: open the row in the Inspector and confirm the Path field exactly matches what `[Capture]` produces — paths are case-sensitive.

---

## Out of scope (today)

- **Inline sprite/glyph in `Text`.** Today, `[action:...]` substitutes the text label only. Rendering an inline sprite mid-string requires sub-batching in the Text render path; tracked as a future enhancement.
- **Dedicated bulk-edit editor window.** The Inspector covers single-asset authoring; large packs (importing a Kenney prompt pack of 200 sprites) currently mean dragging sprites in one row at a time.

See [ActionPromptUI](../ActionPromptUI.md) for the architecture overview if you're extending the system.
