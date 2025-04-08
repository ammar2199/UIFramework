#include "TextView.h"

TextView::TextView(WindowRoot* const window, 
            const std::string& typefaceFile, 
            const std::string& text, int fontSize,
            TextAlignment alignment) : View(window), 
                          mUriTypeface(typefaceFile), mText(text), 
                          mFontSize(fontSize), 
                          mAlignment(alignment),
                          mFontResources(-1), 
                          mTextRGB({1.f, 1.f, 1.f}) {
  mFontResources = GetGraphics()->AddFont(mUriTypeface, 
                                                      mFontSize);
}

void TextView::SetTextRGB(float r, float g, float b) {
  mTextRGB[0] = r;
  mTextRGB[1] = g;
  mTextRGB[2] = b;
}

void TextView::SetText(const std::string& str) {
  mText = str;
}

void TextView::measure() {
   int width = 0;
   int heightAboveBaseline = 0;
   int heightBelowBaseline = 0;
   const GlyphMap2& glyphData = GetGraphics()->GetFontInfo(mFontResources)->glyphMap;
   for (int i=0; i<mText.length(); i++) {
     char c = mText[i];
     GlyphMap2::const_iterator data = glyphData.find(c);
     if (data != glyphData.end()) {
      width += (i == mText.length()-1) ? data->second.width : data->second.horizontalAdvance;
      heightAboveBaseline = std::max(heightAboveBaseline, data->second.baselineYOffset);
      heightBelowBaseline = std::max(heightBelowBaseline, data->second.height - data->second.baselineYOffset);
     }
   }
   mTextBoundingBox.mWidth = width;
   mTextBoundingBox.mHeight = (heightAboveBaseline + heightBelowBaseline);
   mTextBoundingBox.mHeightAboveBaseline = heightAboveBaseline;
  
   // Represents "Content" Box, which is essentially 
   // just padding around the Text Box
   mContentWidth = mTextBoundingBox.mWidth + (2 * mPadding);
   mContentHeight = mTextBoundingBox.mHeight + (2 * mPadding);
}

void TextView::UpdateConstraints() {
  constexpr int ContentConstraintPriority = 1;
  if (!mMeasuredWidthSet) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Width, 
                           Relation::EqualTo, nullptr,
                           BoxAttribute::NoAttribute, 0.f, mContentWidth, 
                           ContentConstraintPriority));
  }
  if (!mMeasuredHeightSet) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Height, 
                           Relation::EqualTo, nullptr,
                           BoxAttribute::NoAttribute, 0.f, mContentHeight, 
                           ContentConstraintPriority));
  }
  mMeasuredWidthSet = true;
  mMeasuredHeightSet = true;
}

void TextView::draw() {
  GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
  GetGraphics()->DrawRect(mLeft, mTop, mRight, mBottom);
  GetGraphics()->SetColor(mOutlineRGB[0], mOutlineRGB[1], 
                                   mOutlineRGB[2], 1.f);
  GetGraphics()->DrawOutline(mLeft, mTop, mRight, mBottom);
  
  // Take Top-Left position of box and subtract padding from it
  // to get top-left position of text bounding box.
  // Then draw at location of the Baseline.
  
  // XXX: 
  // Check. May need to refine this more.
  int baselineX, baselineY;
  switch (mAlignment) {
    case TextAlignment::Center:
      baselineX = ((mLeft + mRight) / 2.0) - (mTextBoundingBox.mWidth/2.0);
      baselineY = ((mTop + mBottom) / 2.0) - (mTextBoundingBox.mHeight/2.0) + 
                  (mTextBoundingBox.mHeightAboveBaseline);
      break;
    case TextAlignment::Left:
      baselineX = mLeft + mPadding;
      baselineY = mTop + mPadding + mTextBoundingBox.mHeightAboveBaseline;
      break;
    case TextAlignment::Right:
      baselineX = mRight - mPadding - mTextBoundingBox.mWidth;
      baselineY = mTop + mPadding + mTextBoundingBox.mHeightAboveBaseline; 
      break;
  }
  

//  baselineX = std::clamp<int>(baselineX, GetLeft() + mPadding, 
//                              GetRight() - mPadding - mTextBoundingBox.mWidth);
  
  // TODO: I think these std::max below only apply for TextAlignment::Left,
  //      might need similar logic for TextAlignment::Right... 
  baselineX = std::max<int>(GetLeft() + mPadding, baselineX);
  baselineY = std::max<int>(mTop + mPadding, baselineY);
  
  Rect originalRect;
  GetGraphics()->SetScissor(GetLeft(), GetTop(), 
                            GetWidth(), GetHeight(), originalRect);
  GetGraphics()->SetColor(mTextRGB[0], mTextRGB[1], 
                                   mTextRGB[2], 1.f);
  GetGraphics()->DrawText(mFontResources, mText, baselineX, baselineY);
  GetGraphics()->UnsetScissor();
  Rect blah;
  GetGraphics()->SetScissor(originalRect, blah);
}
