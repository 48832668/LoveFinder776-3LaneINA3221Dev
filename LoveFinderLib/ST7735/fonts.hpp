/**
 * @file fonts.hpp
 * @brief Font Definitions for LCD Display - C++17
 * @author LoveFinder
 * @date 2026
 * 
 * Provides bitmap fonts for ST7735 LCD display
 */

#ifndef FONTS_HPP
#define FONTS_HPP

#include <cstdint>

/*============================================================================
 * Font Structure
 *============================================================================*/

struct FontDef {
    const uint8_t width;    // Character width in pixels
    const uint8_t height;   // Character height in pixels
    const uint16_t* data;   // Bitmap data array
};

/*============================================================================
 * Available Fonts (extern declarations)
 *============================================================================*/

// Small font: 7x10 pixels
extern const FontDef Font_7x10;

// Medium font: 11x18 pixels
extern const FontDef Font_11x18;

// Large font: 16x26 pixels
extern const FontDef Font_16x26;

// Custom digit font: 9x18 pixels (narrow digits for 3-column current display)
extern const FontDef Font_9x18;

/*============================================================================
 * Font Manager (Optional - for runtime font selection)
 *============================================================================*/

enum class e_Font_Size : uint8_t {
    Small  = 0,   // 7x10
    Medium = 1,   // 11x18
    Large  = 2,   // 16x26
    CustomDigit = 3  // 9x18
};

namespace FontManager {
    /**
     * @brief Get font by size enum
     * @param size Font size
     * @return Reference to font definition
     */
    inline const FontDef& getFont(e_Font_Size size) {
        switch (size) {
            case e_Font_Size::Small:  return Font_7x10;
            case e_Font_Size::Medium: return Font_11x18;
            case e_Font_Size::Large:  return Font_16x26;
            case e_Font_Size::CustomDigit: return Font_9x18;
            default:                  return Font_7x10;
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
