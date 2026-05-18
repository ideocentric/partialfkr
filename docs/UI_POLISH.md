# PartialFKR — UI Polish Reference

> Planning document for the cosmetic pass on buttons, controls, typography, and color.
> All controls use JUCE's default LookAndFeel_V4 unless noted. No custom LookAndFeel subclass exists.

---

## Current Typography

| Location | Font | Size | Weight | Color |
|---|---|---|---|---|
| Panel titles (Filters, Inspector) | JUCE default (macOS: SF Pro, Win: Segoe UI) | 16 pt | **Bold** | JUCE label default |
| Inspector data rows | JUCE default | 13 pt | Regular | JUCE label default |
| Gain readout label | JUCE default | 10 pt | Regular | Transparent BG, default text |
| Gain / dB scale ticks | JUCE default | 9 pt | Regular | `0xff888888` |
| Edit Mode section heading | JUCE default | default | Regular | JUCE label default |
| Time display (transport) | JUCE default | default | Regular | `0xffaaaaaa` |

**No font family is explicitly set anywhere** — all labels inherit the platform system font.
On macOS this is **San Francisco** (SF Pro Text), on Windows **Segoe UI**, on Linux the GTK system font.

### Options for typography polish
- Set an explicit font family via a custom `LookAndFeel` subclass (one place controls everything).
- Common choices for audio apps: **Inter**, **DM Sans**, **JetBrains Mono** (for numeric readouts).
- Alternatively, rely on the system font and just tighten sizes — SF Pro already looks excellent on macOS.

---

## Button Inventory

### Edit Mode: Select / Direct Select

| Property | Current |
|---|---|
| Type | `juce::TextButton` |
| Size | 28 px tall, ~half panel width each |
| Text | "Select" / "Direct Select" |
| Inactive BG | `0xff1a1a1a` (near black) |
| Inactive text | Grey |
| Active BG | `0xff606060` (medium grey) |
| Active text | White |
| Keyboard hints | V key / A key (shown only in tooltip/docs) |

**Unicode options**

| Symbol | Code point | Notes |
|---|---|---|
| `↖` | U+2196 | Upper-left arrow — generic "select" |
| `⬚` | U+2B1A | Hollow box with dot — direct select feel |
| `▷` | U+25B7 | Hollow triangle — less obvious |
| Keep text | — | "V" / "A" abbreviations with font size 11–12 pt would be compact and match DAW conventions |

**Recommended approach**: abbreviate to **"V"** and **"A"** (matching Adobe/DAW convention), optionally prefix a small Unicode glyph. Keeps the button narrow.

**SVG spec** (if going full icon route):
- Canvas: **20 × 20 px** (fits the 28 px button height with 4 px top/bottom padding)
- Cursor arrow (Select): standard pointer arrow, 14 px tall, 1.5 px stroke, filled tip
- Hollow arrow (Direct Select): same outline, unfilled interior, 1.5 px stroke
- Color: white on dark BG; tint to match active/inactive state via `DrawableButton::ColourScheme`

---

### Reset Button

| Property | Current |
|---|---|
| Type | `juce::TextButton` |
| Size | 28 px tall, full panel width |
| Text | "Reset" |
| Colors | JUCE LookAndFeel defaults (no explicit override) |

**Unicode options**

| Symbol | Code point | Notes |
|---|---|---|
| `↺` | U+21BA | Counter-clockwise arrow — clear "reset" metaphor |
| `⟳` | U+27F3 | Clockwise circle arrow |
| `↻` | U+21BB | Clockwise arrow |
| `⌫` | U+232B | Delete backward — less appropriate |

**Recommended**: `↺` Reset  (symbol + word) at 12–13 pt, or symbol-only at 16 pt if space allows.

**SVG spec**:
- Canvas: **20 × 20 px**
- Circular arrow, radius 7 px, 2 px stroke, open arc ~300°, arrowhead at terminus
- Matches the Loop button aesthetic (which already draws a similar arc)

---

### Zoom Buttons (+ / −)

| Property | Current |
|---|---|
| Type | `ZoomButton` (custom `juce::Button` subclass) |
| Drawing | Custom `paint()` — geometric rounded rectangles |
| Circle BG (normal) | `0xff2a2a2a` |
| Circle BG (hover) | `0xff3a3a3a` |
| Circle BG (pressed) | `0xff4a4a4a` |
| Border | `0xff606060`, 0.8 px stroke |
| Symbol color (enabled) | Light grey (`0xffcccccc` area) |
| Symbol color (disabled) | `0xff444444` |
| Plus: two rounded rects | arm = radius × 0.55, thickness = radius × 0.18 |
| Minus: one rounded rect | same proportions, horizontal bar only |

These buttons **already look polished** — custom-drawn vector shapes, dark theme consistent.

**Possible refinements only**:
- Slightly increase the symbol brightness when hovered (step from `0xff2a2a2a` to something a bit brighter than `0xff3a3a3a`)
- Add a subtle outer glow or ring highlight on hover instead of the flat fill step

**No SVG needed** — already drawn in code. Exposed colors are constants in `ZoomButton.h`; easy to tune.

---

### Transport: Stop / Play / Pause / Loop

| Property | Current |
|---|---|
| Type | `TransportButton` (custom `juce::Button` subclass) |
| Size | 36 × 36 px each |
| Drawing | Custom `paint()` — all geometric |
| BG (normal) | `0xff1a1a1a` |
| BG (hover) | `0xff2e2e2e` |
| BG (pressed) | `0xff484848` |
| Corner radius | 3 px |
| Icon (enabled, normal) | `0xffcccccc` |
| Icon (enabled, hover/press) | `0xffffffff` (white) |
| Icon (disabled) | `0xff484848` |
| Loop ON accent | `0xff44cc44` (green) |

**Stop**: filled square, side = dim × 0.27  
**Play**: filled triangle, radius = dim × 0.30  
**Pause**: two filled bars, bar W = dim × 0.11, bar H = dim × 0.44  
**Loop**: open arc ~270°, sweep W = dim × 0.09, arrowhead at tip  

These buttons are **production-quality already**. Possible refinements:
- Slightly rounder corner radius (3 → 5 px) for a softer feel
- Loop accent color — could match a brand color instead of generic green
- Consider a thin 1 px ring border on the buttons (currently borderless), matching the ZoomButton style

---

## Sliders

All six panel sliders share the same configuration:

| Property | Current |
|---|---|
| Type | `juce::Slider::LinearHorizontal` |
| Text box | Right-aligned, 60 × 20 px, editable |
| Track height | JUCE LookAndFeel default (~4–6 px) |
| Thumb | JUCE LookAndFeel default circular thumb |
| Label | `juce::Label` above each slider, 18 px tall |

### Slider color options (via `setColour()`)

JUCE's `Slider` exposes these colour IDs for direct customisation:

| ColourID | What it controls | Suggested value |
|---|---|---|
| `Slider::thumbColourId` | Circular thumb fill | `0xffffffff` (white) or accent color |
| `Slider::trackColourId` | Filled portion of track (left of thumb) | `0xff606060` or accent |
| `Slider::backgroundColourId` | Unfilled track background | `0xff2a2a2a` |
| `Slider::textBoxTextColourId` | Value text color | `0xffdddddd` |
| `Slider::textBoxBackgroundColourId` | Text box background | `0xff1a1a1a` |
| `Slider::textBoxOutlineColourId` | Text box border | `0xff000000` (invisible) or `0xff333333` |
| `Slider::textBoxHighlightColourId` | Selection highlight in text box | `0xff4488ff` |

**Recommended defaults for the dark theme**:
```cpp
slider.setColour(juce::Slider::thumbColourId,            juce::Colour(0xffffffff));
slider.setColour(juce::Slider::trackColourId,            juce::Colour(0xff505050));
slider.setColour(juce::Slider::backgroundColourId,       juce::Colour(0xff252525));
slider.setColour(juce::Slider::textBoxTextColourId,      juce::Colour(0xffcccccc));
slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff1a1a1a));
slider.setColour(juce::Slider::textBoxOutlineColourId,   juce::Colour(0xff000000));
```

Apply to all sliders at once via a custom LookAndFeel rather than per-slider calls.

---

## Scroll / Viewport Controls

The partial canvas (`PartialView`) uses a custom zoom/pan model — no JUCE `Viewport` or `ScrollBar` component is involved. Scrolling is handled entirely by internal time/frequency range state, so no scroll bar color customization is needed or available.

---

## Custom LookAndFeel — Recommended Approach

Rather than calling `setColour()` on every individual control, create a single `PartialFKRLookAndFeel` subclass. Benefits:
- One place for all font, color, and shape overrides
- Can override `drawButtonBackground()`, `drawLinearSlider()`, `drawLinearSliderThumb()` for richer visuals
- Installed once in `Main.cpp` via `juce::LookAndFeel::setDefaultLookAndFeel(&lf)`

### Minimal starting skeleton

```cpp
// Source/UI/PartialFKRLookAndFeel.h
class PartialFKRLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PartialFKRLookAndFeel()
    {
        // Slider colours applied globally
        setColour(juce::Slider::thumbColourId,             juce::Colour(0xffffffff));
        setColour(juce::Slider::trackColourId,             juce::Colour(0xff505050));
        setColour(juce::Slider::backgroundColourId,        juce::Colour(0xff252525));
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffcccccc));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1a1a));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xff000000));

        // TextButton colours
        setColour(juce::TextButton::buttonColourId,        juce::Colour(0xff2a2a2a));
        setColour(juce::TextButton::textColourOffId,       juce::Colour(0xffaaaaaa));
        setColour(juce::TextButton::textColourOnId,        juce::Colour(0xffffffff));
    }

    // Override font used across all controls
    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(juce::FontOptions{}.withHeight(13.0f));
    }
};
```

Install in `Main.cpp` `initialise()`:
```cpp
static PartialFKRLookAndFeel lf;
juce::LookAndFeel::setDefaultLookAndFeel(&lf);
```

---

## Implementation Priority

| Item | Effort | Impact |
|---|---|---|
| Slider color tuning (thumb/track) | Low — a few `setColour` calls | Medium — sliders look more intentional |
| Custom LookAndFeel skeleton | Low — one new file | High — foundation for all future polish |
| Edit Mode button abbreviation (V / A) | Trivial — change two strings | Low — minor readability gain |
| Reset Unicode symbol | Trivial — one string change | Low |
| SVG icon system (DrawableButton) | High — new asset pipeline | High — fully custom look |
| Custom slider thumb paint override | Medium — one `drawLinearSliderThumb` override | Medium |
| Transport/Zoom color micro-tweaks | Low — change a few constants | Low |