#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "Image.h"

// Vulkan
#include <vulkan/vulkan.h>

// Freetype
#include <ft2build.h>
#include FT_FREETYPE_H

// Note: Ignores ligatures. One Glyph per Character.
struct Glyph2 {
  char character; // ASCII character code
  int width; // width of glyph in font atlas
  int height; // height of glyph in font atlas
  int startX, startY; // Top-left position in font-atlas
  int baselineYOffset; // Y of baseline for glyph. (0 is top of glyph)
  int horizontalAdvance;
};

using GlyphMap2 = std::unordered_map<char, Glyph2>;

// eg:
// Font: Arial-Regular
struct FontInfo {
  FontInfo& operator=(FontInfo&&) = default;
  int linespace; // Unused for now, or is it? I think it is being used.
  int ascender; // I use this for setting the Cursor Height.
  int atlasWidth; // Width and Height of Font Atlas
  int atlasHeight;
  Image fontAtlas; // Font Atlas
  GlyphMap2 glyphMap;
};

using FontId_t = int;


namespace std {
template<>
struct hash<std::pair<std::string, int>> {
  std::size_t operator()(const std::pair<std::string, int>& pair) const {
    return std::hash<std::string>{}(pair.first) | std::hash<int>{}(pair.second);
  }
};
}

// Handles FreeType.
// Is also capable of loading and unloading 
// images onto GPU.
class Text {
  static int HorizontalDPI;
  static int VerticalDPI;
 public:
   Text();
   Text(VkDevice device, VkPhysicalDevice phyDev);

   ~Text();

   Text& operator=(const Text&) = delete;
   Text& operator=(Text&&) = default;

   FontId_t AddFont(const std::string& typefaceFile, 
                    const uint32_t pointSize, 
                    VkQueue queue, VkCommandBuffer);

   void Destroy() {
    mFontMap.clear();
   }

   Glyph2 GetGlyphInfo(const int FontId_t, const char character);
    
   const FontInfo* GetFontInfo(const FontId_t id) const;
   
   void SetDevice(VkDevice device) {
     mDevice = device;
   }

   void SetPhysicalDevice(VkPhysicalDevice phyDev) {
     mPhysicalDevice = phyDev;
   }

 private:
  static void WriteImgToPPM(const char* filename, const uint8_t* buffer, int width, int height);
  VkPhysicalDevice mPhysicalDevice;
  VkDevice mDevice;
  std::unordered_map<FontId_t, FontInfo> mFontMap;
  std::unordered_map<std::pair<std::string, int>, FontId_t> mFontIdMap;
  FT_Library mFTLibrary;
};

