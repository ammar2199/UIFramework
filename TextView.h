#pragma once

#include "View.h"
#include <array>
#include <string>

class TextView : public View {
 public:
  enum class TextAlignment {
     Left,
     Center, 
     Right
  };

  TextView(WindowRoot* const window, 
          const std::string& typefaceFile, 
          const std::string& text, int fontSize = 20, 
          TextAlignment alignment = TextAlignment::Center);
 
  virtual void measure() override;
  virtual void UpdateConstraints() override;
  virtual void draw() override;
   
  void SetTextRGB(float r, float g, float b);
  void SetText(const std::string& s);

 private:
   std::string mUriTypeface;
   std::string mText;
   int mFontSize; // Unit in points. 1/72 of an inch.
   FontId_t mFontResources{-1};
   std::array<float, 3> mTextRGB;
   
   bool mMeasuredWidthSet{false};
   bool mMeasuredHeightSet{false};
   
   TextAlignment mAlignment;
   int mPadding = 10;
   struct {
     int mWidth;
     int mHeight;
     int mHeightAboveBaseline; 
   } mTextBoundingBox;
};

