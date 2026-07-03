/**
 * @file fonts.hpp
 * @brief Font Definitions for LCD Display - C++17
 * @author LoveFinder
 * @date 2026
 * 
 * Provides bitmap fonts for ST7735 LCD display.
 * Font selection is controlled by USE_FONT_STYLE1_* / USE_FONT_STYLE2_* defines
 * from fonts_config.hpp — only enabled fonts are compiled into the firmware.
 */

#ifndef FONTS_HPP
#define FONTS_HPP

#include <cstdint>
#include "fonts_config.hpp"

/*============================================================================
 * Font Structure
 *============================================================================*/

struct FontDef {
    const uint8_t width;    // Character width in pixels
    const uint8_t height;   // Character height in pixels
    const uint16_t* data;   // Bitmap data array
};

/*============================================================================
 * Available Fonts (extern declarations — conditional)
 *============================================================================*/

// ── Style 1: Classic proportional fonts ────────────────────────────────

#if USE_FONT_STYLE1_7X10
// Small font: 7x10 pixels (badges, debug screens)
extern const FontDef Font_Style1_7x10;
#endif

#if USE_FONT_STYLE1_11X18
// Medium font: 11x18 pixels
extern const FontDef Font_Style1_11x18;
#endif

#if USE_FONT_STYLE1_16X26
// Large font: 16x26 pixels
extern const FontDef Font_Style1_16x26;
#endif

// ── Style 2: Custom narrow-digit fonts ─────────────────────────────────

#if USE_FONT_STYLE2_9X18
// Narrow font: 9x18 pixels (main display — all 4 rows)
extern const FontDef Font_Style2_9x18;
#endif

// ── Style 3: Chinese bitmap font ─────────────────────────────────────

#if USE_FONT_STYLE3_ZH_16X16
// Chinese font: 16x16 pixels (splash screen)
extern const FontDef Font_Style3_ZH_16x16;
#endif

/*============================================================================
 * Glyph Lookup — Per-Character Conditional Access
 *============================================================================*/

/**
 * @brief Get glyph bitmap data for a character in the given font.
 * 
 * This replaces direct array indexing of font.data[].
 * Each font stores only its used characters; missing chars return nullptr.
 * 
 * @param font Font definition
 * @param ch   ASCII code (32-126)
 * @return Pointer to height uint16_t values, or nullptr if glyph absent
 */
const uint16_t* font_get_glyph(const FontDef& font, uint8_t ch);

/**
 * @brief Get glyph bitmap data for a Unicode character (used by Chinese fonts).
 * @param font Font definition
 * @param uni  Unicode code point (e.g. 0x4F60 = 你, 0x597D = 好)
 * @return Pointer to height uint16_t values, or nullptr if glyph absent
 */
const uint16_t* font_get_glyph_unicode(const FontDef& font, uint16_t uni);

/*============================================================================
 * Font Manager (Optional — for runtime font selection)
 *============================================================================*/

enum class e_Font_Size : uint8_t {
#if USE_FONT_STYLE1_7X10
    Style1_7x10 = 0,
#endif
#if USE_FONT_STYLE1_11X18
    Style1_11x18 = 1,
#endif
#if USE_FONT_STYLE1_16X26
    Style1_16x26 = 2,
#endif
#if USE_FONT_STYLE2_9X18
    Style2_9x18 = 3,
#endif
#if USE_FONT_STYLE3_ZH_16X16
    Style3_ZH_16x16 = 4,
#endif
};

namespace FontManager {
    /**
     * @brief Get font by size enum
     * @param size Font size
     * @return Reference to font definition
     */
    inline const FontDef& getFont(e_Font_Size size) {
        switch (size) {
#if USE_FONT_STYLE1_7X10
            case e_Font_Size::Style1_7x10:  return Font_Style1_7x10;
#endif
#if USE_FONT_STYLE1_11X18
            case e_Font_Size::Style1_11x18: return Font_Style1_11x18;
#endif
#if USE_FONT_STYLE1_16X26
            case e_Font_Size::Style1_16x26: return Font_Style1_16x26;
#endif
#if USE_FONT_STYLE2_9X18
            case e_Font_Size::Style2_9x18:  return Font_Style2_9x18;
#endif
#if USE_FONT_STYLE3_ZH_16X16
            case e_Font_Size::Style3_ZH_16x16: return Font_Style3_ZH_16x16;
#endif
            default:
#if USE_FONT_STYLE1_7X10
                return Font_Style1_7x10;
#else
                // If no fonts are enabled, this will fail at link time
                return *static_cast<const FontDef*>(nullptr);
#endif
        }
    }
    
    /**
     * @brief Calculate text width
     * @param text Text string
     * @param font Font to use
     * @return Width in pixels
     */
    inline uint16_t getTextWidth(const char* text, const FontDef& font) {
        uint16_t width = 0;
        while (*text) {
            width += font.width;
            text++;
        }
        return width;
    }
    
    /**
     * @brief Calculate text height
     * @param font Font to use
     * @return Height in pixels
     */
    inline uint8_t getTextHeight(const FontDef& font) {
        return font.height;
    }
}

#endif // FONTS_HPP
