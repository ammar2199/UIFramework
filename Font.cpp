#include "Font.h"

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

// Vulkan
#include <vulkan/vulkan.h>

// Freetype.
#include <ft2build.h>
#include FT_FREETYPE_H

// FreeType Metrics are stored in 26.6 "fractional-pixel" Fixed-Point Format, 
// hence we divide by 64 to get whole pixels. Could also shift right...
#define WHOLE_TO_FIXED(x) (x * 64)
#define FIXED_TO_WHOLE(x) (x / 64.0)

int Text::HorizontalDPI = 196;
int Text::VerticalDPI = 196;

Text::Text() {
  FT_Error error = FT_Init_FreeType(&mFTLibrary);
  if (error) {
    throw std::runtime_error("Failed to Initialize FreeType");
  }
}

Text::~Text() {
  Destroy();
  FT_Done_FreeType(mFTLibrary);
}

Text::Text(VkDevice device, VkPhysicalDevice phyDev) : Text() {
  SetPhysicalDevice(phyDev);
  SetDevice(device);
}

FontId_t Text::AddFont(const std::string& typefaceFile, const uint32_t pointSize, VkQueue queue, VkCommandBuffer commandBuffer) {
  static int fontIds = 1;
  if (mFontIdMap.find({typefaceFile, pointSize}) != mFontIdMap.end()) {
    return mFontIdMap[{typefaceFile, pointSize}];
  }

  FontId_t fontID = fontIds++;
  
  FT_Face ftFace;
  FT_Error err = FT_New_Face(mFTLibrary, typefaceFile.c_str(), 0, &ftFace);
  if (err) {
    throw std::runtime_error(std::string("Failed to create FT_Face for ") + typefaceFile); 
  }
  
  GlyphMap2 glyphMap; // Build GlyphMap.

  if (FT_Set_Char_Size(ftFace, 0, WHOLE_TO_FIXED(pointSize), Text::HorizontalDPI, Text::VerticalDPI)) {
    throw std::runtime_error("FreeType Fail: Could not set character size.");
  }
  
  FontInfo fontInfo;
  // XXX: Do I really need the division by the double?
  // TODO in the conversion?
  fontInfo.linespace = FIXED_TO_WHOLE(ftFace->size->metrics.height);
  fontInfo.ascender = FIXED_TO_WHOLE(ftFace->size->metrics.ascender);

  uint64_t atlasWidth = 0;
  uint64_t atlasHeight = 0;
  constexpr int xOffset = 2;
  const FT_ULong startCode = 32;
  const FT_ULong endCode = 127;

  for (FT_ULong charcode = startCode; charcode <= endCode; charcode++) {
    FT_UInt glyphIndex = FT_Get_Char_Index(ftFace, charcode); 
    
   if (FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_RENDER)) {
    continue;
   }
   atlasWidth += ftFace->glyph->bitmap.width + xOffset;
   atlasHeight = std::max<uint64_t>(atlasHeight, ftFace->glyph->bitmap.rows);
  }
  
  uint8_t* atlasBuffer = (uint8_t*)calloc(atlasWidth * atlasHeight, 1);
  uint32_t glyphOffset = 0;
  for (FT_ULong charcode = startCode; charcode <= endCode; charcode++) {
    FT_UInt glyphIndex = FT_Get_Char_Index(ftFace, charcode); 
    if (FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_RENDER)) {
      continue;
    }
    // Copy over.
    for (int row=0; row < ftFace->glyph->bitmap.rows; row++) {
      for (int col=0; col < ftFace->glyph->bitmap.width; col++) {
        atlasBuffer[row * atlasWidth + col + glyphOffset] = ftFace->glyph->bitmap.buffer[ftFace->glyph->bitmap.pitch * row + col];
      }
    } 

    Glyph2 glyph;
    glyph.character = static_cast<char>(charcode);
    glyph.width = ftFace->glyph->bitmap.width;
    glyph.height = ftFace->glyph->bitmap.rows;
    glyph.startX = glyphOffset; 
    glyph.startY = 0;
    glyph.baselineYOffset = FIXED_TO_WHOLE(ftFace->glyph->metrics.horiBearingY);
    glyph.horizontalAdvance = FIXED_TO_WHOLE(ftFace->glyph->metrics.horiAdvance);
    
    // XXX: SPACE-CHARACTER HACK
    if (glyph.character == ' ') {
      glyph.width = glyph.horizontalAdvance;
    }

    fontInfo.glyphMap[charcode] = glyph;
    glyphOffset += ftFace->glyph->bitmap.width + xOffset; 
  } 
  fontInfo.atlasWidth = atlasWidth;
  fontInfo.atlasHeight = atlasHeight;
  
  Image image(mPhysicalDevice, mDevice, atlasWidth, atlasHeight, 1);
  image.Load(queue, commandBuffer, atlasBuffer);
  // For quick inspection:
  //WriteImgToPPM("atlas.ppm", atlasBuffer, atlasWidth, atlasHeight);
  free(atlasBuffer);
  fontInfo.fontAtlas = std::move(image);
  mFontMap[fontID] = std::move(fontInfo);
  mFontIdMap[{typefaceFile, pointSize}] = fontID;
  return fontID;
}

// XXX: Not visible by eog? Something needs to be fixed.
void Text::WriteImgToPPM(const char *filename, const uint8_t* img, int width, int height) {
  std::ofstream output(filename);
  if (!output) {
    std::cerr << "Failed to create PPM file";
    return;
  }
  output << "P6\n" << width << ' ' << height << '\n'; 
  output << 255 /*Max value*/ << '\n';
  for (int i = 0; i < height * height; i++) {
    output.put(img[i]);
    output.put(img[i]);
    output.put(img[i]);
  }
}

const FontInfo* Text::GetFontInfo(const FontId_t id) const {
  auto iter = mFontMap.find(id);
  if (iter == mFontMap.end()) return nullptr;
  return &(iter->second);
}

