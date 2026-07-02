/**
 * @file fonts_config.hpp
 * @brief Font Compilation Configuration
 * @author LoveFinder
 * @date 2026
 *
 * Controls which fonts are compiled into the firmware.
 * Within each enabled font, only the glyphs for characters actually used
 * in the code are compiled — unused characters cost zero Flash.
 * Set these defines in your build system (uvprojx <Define>) or
 * override them before including this header to enable/disable specific fonts.
 *
 * Default: Font_Style1_7x10 (badges, debug) + Font_Style2_9x18 (main display) enabled.
 *          Font_Style1_11x18 and Font_Style1_16x26 disabled (not used in current firmware).
 *
 * Per-character selection is automatic — fonts.cpp declares arrays only for
 * characters used in main.cpp (e.g., Font_Style2_9x18: digits, ., A, C, H, V, W, h, m;
 * Font_Style1_7x10: space, !, 0-9, A-F, I, N, W, c-x).
 */

#ifndef FONTS_CONFIG_HPP
#define FONTS_CONFIG_HPP

// ------------------------------------------------------------------
// Style 1 — Classic proportional fonts (original afiskon/stm32-st7735)
// ------------------------------------------------------------------
#ifndef USE_FONT_STYLE1_7X10
#define USE_FONT_STYLE1_7X10    1   // 7×10  — unit badges, error messages, I2C scan
#endif

#ifndef USE_FONT_STYLE1_11X18
#define USE_FONT_STYLE1_11X18   0   // 11×18 — not currently used (retired)
#endif

#ifndef USE_FONT_STYLE1_16X26
#define USE_FONT_STYLE1_16X26   0   // 16×26 — disabled (Flash space limitation)
#endif

// ------------------------------------------------------------------
// Style 2 — Custom narrow-digit fonts
// ------------------------------------------------------------------
#ifndef USE_FONT_STYLE2_9X18
#define USE_FONT_STYLE2_9X18    1   // 9×18  — main display font (all 4 rows)
#endif

#endif // FONTS_CONFIG_HPP
