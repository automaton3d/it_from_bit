/*
 * color_utils.h
 * Utility functions for encoding and decoding RGB colors
 */

#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include <cstdint>
#include <algorithm>

/**
 * Encodes RGB float values (0.0 to 1.0) into a 32-bit integer
 * Format: 0x00RRGGBB
 * 
 * @param r Red component (0.0 to 1.0)
 * @param g Green component (0.0 to 1.0)
 * @param b Blue component (0.0 to 1.0)
 * @return 32-bit encoded color
 */
inline uint32_t encodeColor(float r, float g, float b)
{
  // Clamp values to [0.0, 1.0] range
  r = std::max(0.0f, std::min(1.0f, r));
  g = std::max(0.0f, std::min(1.0f, g));
  b = std::max(0.0f, std::min(1.0f, b));

  // Convert to 8-bit values (0-255)
  uint8_t r8 = static_cast<uint8_t>(r * 255.0f);
  uint8_t g8 = static_cast<uint8_t>(g * 255.0f);
  uint8_t b8 = static_cast<uint8_t>(b * 255.0f);

  // Pack into 32-bit integer: 0x00RRGGBB
  return (static_cast<uint32_t>(r8) << 16) |
         (static_cast<uint32_t>(g8) << 8)  |
         (static_cast<uint32_t>(b8));
}

/**
 * Encodes RGB byte values (0 to 255) into a 32-bit integer
 * Format: 0x00RRGGBB
 * 
 * @param r Red component (0 to 255)
 * @param g Green component (0 to 255)
 * @param b Blue component (0 to 255)
 * @return 32-bit encoded color
 */
inline uint32_t encodeColorByte(uint8_t r, uint8_t g, uint8_t b)
{
  return (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8)  |
         (static_cast<uint32_t>(b));
}

/**
 * Decodes a 32-bit color into RGB float components (0.0 to 1.0)
 * 
 * @param color 32-bit encoded color (0x00RRGGBB)
 * @param r Output red component
 * @param g Output green component
 * @param b Output blue component
 */
inline void decodeColor(uint32_t color, float& r, float& g, float& b)
{
  uint8_t r8 = (color >> 16) & 0xFF;
  uint8_t g8 = (color >> 8) & 0xFF;
  uint8_t b8 = color & 0xFF;

  r = static_cast<float>(r8) / 255.0f;
  g = static_cast<float>(g8) / 255.0f;
  b = static_cast<float>(b8) / 255.0f;
}

/**
 * Decodes a 32-bit color into RGB byte components (0 to 255)
 * 
 * @param color 32-bit encoded color (0x00RRGGBB)
 * @param r Output red component
 * @param g Output green component
 * @param b Output blue component
 */
inline void decodeColorByte(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
  r = (color >> 16) & 0xFF;
  g = (color >> 8) & 0xFF;
  b = color & 0xFF;
}

/**
 * Creates a grayscale color from a single intensity value
 * 
 * @param intensity Grayscale intensity (0.0 to 1.0)
 * @return 32-bit encoded grayscale color
 */
inline uint32_t encodeGrayscale(float intensity)
{
  return encodeColor(intensity, intensity, intensity);
}

/**
 * Blends two colors using linear interpolation
 * 
 * @param color1 First color
 * @param color2 Second color
 * @param t Blend factor (0.0 = color1, 1.0 = color2)
 * @return Blended color
 */
inline uint32_t blendColors(uint32_t color1, uint32_t color2, float t)
{
  float r1, g1, b1, r2, g2, b2;
  decodeColor(color1, r1, g1, b1);
  decodeColor(color2, r2, g2, b2);

  float r = r1 + (r2 - r1) * t;
  float g = g1 + (g2 - g1) * t;
  float b = b1 + (b2 - b1) * t;

  return encodeColor(r, g, b);
}

/**
 * Common color constants
 */
namespace Colors {
  const uint32_t BLACK   = 0x000000;
  const uint32_t WHITE   = 0xFFFFFF;
  const uint32_t RED     = 0xFF0000;
  const uint32_t GREEN   = 0x00FF00;
  const uint32_t BLUE    = 0x0000FF;
  const uint32_t YELLOW  = 0xFFFF00;
  const uint32_t CYAN    = 0x00FFFF;
  const uint32_t MAGENTA = 0xFF00FF;
  const uint32_t GRAY    = 0x808080;
  const uint32_t ORANGE  = 0xFF8000;
  const uint32_t PURPLE  = 0x8000FF;
}

#endif // COLOR_UTILS_H