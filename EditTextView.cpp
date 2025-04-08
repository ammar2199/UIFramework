#include "EditTextView.h"

#include "Font.h"
#include "Timer.h"

#include <array>
#include <cassert>
#include <vector>

EditTextView::EditTextView(WindowRoot* const window, 
               const std::string& typefaceFile, 
               int fontSize) : View(window), 
               mTypeface(typefaceFile), mFontSize(fontSize), 
               mCursorPosition(0), mLoaded(false), 
               mViewportY(0), mMaxViewportY(0), 
               mTextRGB({0.f, 0.f, 0.f}), 
               mCursorRGB({0.f, 0.f, 0.f}),
               mCursorWidth(5),
               mCursorTimer(window, 1.0) {
    SetRGB(1.f , 1.f, 1.f);
    SetOutlineRGB(0.f, 0.f, 0.f);
    // Load Font Resources.
    mFontResources = GetGraphics()->AddFont(mTypeface, mFontSize);
    mLoaded = true;
    mLineSpace = GetGraphics()->GetFontInfo(mFontResources)->linespace;
    mCursorHeight = GetGraphics()->GetFontInfo(mFontResources)->ascender;
    mCursorTimer.Start();
}

void EditTextView::SetTextRGB(float r, float g, float b) {
  mTextRGB[0] = r;
  mTextRGB[1] = g;
  mTextRGB[2] = b;
}

void EditTextView::SetCursorRGB(float r, float g, float b) {
  mCursorRGB[0] = r;
  mCursorRGB[1] = g;
  mCursorRGB[2] = b;
}

void EditTextView::SetText(const std::string& str) {
  mContent = str;
  mCursorPosition = 0;
}

void EditTextView::SetTextSize(int pointSize) {
  assert(pointSize > 0 && pointSize < 100 && "Point size not in range: [0, 100]");
  if (pointSize != mFontSize) {
    mFontSize = pointSize;
    mFontResources = GetGraphics()->AddFont(mTypeface, mFontSize);
    mLineSpace = GetGraphics()->GetFontInfo(mFontResources)->linespace;
    mCursorHeight = GetGraphics()->GetFontInfo(mFontResources)->ascender;
  }
} 

void EditTextView::measure() {
}

void EditTextView::layout(int l, int t, int r, int b) {
  mLeft = l, mTop = t, mRight = r, mBottom = b;
  mWidth = mRight - mLeft;
  mHeight = mBottom - mTop;
 
  // Place Text into distinct lines of text.
  mLineContent.clear();
  const GlyphMap2& glyphMap = GetGraphics()->GetFontInfo(mFontResources)->glyphMap;
  int tempL = GetLeft();
  std::string line;
  bool cursorSeen = false;
  for (int i=0; i<mContent.length(); i++) {
    if (mCursorPosition == i && !cursorSeen) {
      cursorSeen = true;
      mCursorLine = mLineContent.size();
      mCursorBaselineX = tempL;
      tempL += mCursorWidth + 5; // XXX: 5 is just the advance width of cursor
                                 // it's also seen below in EditTextView::draw...
                                 // Arbitrarily chosen for now.
      i--; // Reprocess this character.
      continue;
    }

    if (mContent[i] == '\n') {
      // End this line, move onto next character

      // Note: I am going to push the '\n' 
      // character into the end of this line
      // so that within EditTextView::draw()
      // we can determine how many characters
      // we've passed over. (charactersSeen)
      // This is necessary since mCursorPosition
      // is based off of the original string,
      // which includes '\n'.
      line.push_back('\n');
      mLineContent.push_back(std::move(line));
      tempL = GetLeft();
      line.clear();
      continue;
    }

    auto iter = glyphMap.find(mContent[i]);
    if (iter == glyphMap.end()) { // Skip to next character.
      continue;
    }
    const Glyph2& glyph = iter->second;
    tempL += glyph.horizontalAdvance;
    
    if (tempL >= GetRight() && line.length() > 0) {
      mLineContent.push_back(std::move(line));
      line.clear();
      tempL = GetLeft() + glyph.horizontalAdvance;
    }
    line += mContent[i]; // Add character to this line.
  }
  
  if (!line.empty()) {
    mLineContent.push_back(line);
    if (!cursorSeen) {
      mCursorLine = mLineContent.size() - 1; // Cursor must be 
                                           // in last position.
      mCursorBaselineX = tempL; 
    }
  }

  if (mLineContent.empty()) { // Occurs when mContent is empty.
    mLineContent.push_back(""); // Push empty line.
    mCursorLine = 0;
    mCursorBaselineX = GetLeft();
  }
  mMaxViewportY = std::max<int>(0, (GetTop() + ((mLineContent.size() + 1) * mLineSpace)) - GetBottom());
}

void EditTextView::draw() {
  GetGraphics()->UnsetScissor();
  // Draw Background.
  Rect originalScissor; // TODO: Reset this
  GetGraphics()->SetScissor(GetLeft(), GetTop(), 
                            GetWidth(), GetHeight(), 
                            originalScissor);
  GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
  GetGraphics()->DrawRect(mLeft, mTop, 
                                  mRight, mBottom);

  GetGraphics()->SetColor(mTextRGB[0], mTextRGB[1], mTextRGB[2], 1.f);

  // Then draw each line using 2D-DrawText.
  // Move to next line using line-spacing from Global Glyph Metric Info.
  // Draw all lines in View's Viewport and first lines outside of Viewport.
  int baselineX = GetLeft();
  int baselineY = (GetTop() + mLineSpace) - mViewportY;
  int lineNum = 0, charactersSeen = 0; // Used for to determine line of cursor.
  for (const auto& line : mLineContent) {
    if (baselineY + mLineSpace < GetTop()) { // Skip, so that we only Draw first-line above Viewport
      baselineY += mLineSpace;
      charactersSeen += line.length(); // Includes LF character, if there is one.
      lineNum++;
      continue;
    }

    if (mCursorLine == lineNum) {
      int strSize = 0;
      if (!line.empty()) {
        strSize = (line.back() == '\n') ? line.size() : line.size()-1;
      }

      // Draw First half of line
      GetGraphics()->DrawText(mFontResources, 
                                      line.substr(0, mCursorPosition - charactersSeen), 
                                      baselineX, baselineY);
      // Draw Cursor
      // XXX: Is there a better way to express this?
      if (IsFocusedView() && (mCursorTimer.IntervalsPassed(mWindow->GetCurrentTime())) % 2 == 1) {
        GetGraphics()->SetColor(mCursorRGB[0], mCursorRGB[1], mCursorRGB[2], 1.f);
        GetGraphics()->DrawRect(mCursorBaselineX, baselineY - mCursorHeight,
                                       mCursorBaselineX + mCursorWidth, baselineY);
        GetGraphics()->SetColor(mTextRGB[0], mTextRGB[1], mTextRGB[2], 1.f);
      }

      // Draw second half of line.
      if ((mCursorPosition-charactersSeen) < line.length()) {
        // the + 5 s for horizontal advance of cursor. - essentially
        // just chosen by me to provide some spacing.
        GetGraphics()->DrawText(mFontResources, 
                                      line.substr(mCursorPosition - charactersSeen, 
                                      strSize),
                                      mCursorBaselineX + mCursorWidth + 5, baselineY);
      }
    } else if (!line.empty()) {
      int strSize = (line.back() == '\n') ? line.size() : line.size()-1;
      // Draw Line
      GetGraphics()->DrawText(mFontResources, line.substr(0, strSize), baselineX, baselineY);
    }
    
    lineNum++;
    charactersSeen += line.length(); // Includes LF character, if there is one.
    baselineY += mLineSpace; // Move to next line.
   
    if (baselineY - mLineSpace > GetBottom()) { // Break out after drawing first
                                                // non-visible baseline.
      break;
    }
  }

  GetGraphics()->UnsetScissor();
  GetGraphics()->SetColor(mOutlineRGB[0], mOutlineRGB[1], mOutlineRGB[2], 1.f);
  GetGraphics()->DrawOutline(mLeft, mTop, mRight, mBottom);
}

void EditTextView::InjectInputEvent(const InputEvent& e) {
  if (e.type == InputType::Text) {
    auto before = mContent.substr(0, mCursorPosition);
    before += e.character;
    before += mContent.substr(mCursorPosition, mContent.size());
    mContent = std::move(before); 
    mCursorPosition += 1;
  } else if (e.type == InputType::Key) {
    if (e.keycode == Key::Backspace) {
      if (!mContent.empty() && mCursorPosition > 0) {
       auto before = mContent.substr(0, mCursorPosition - 1);
       before += mContent.substr(mCursorPosition, mContent.size());
       mContent = std::move(before);
       mCursorPosition -= 1;
      }
    } else if (e.keycode == Key::RightArrow) {
      mCursorPosition = std::min<int>(mCursorPosition + 1,
                                     mContent.size());
    } else if (e.keycode == Key::LeftArrow) {
      mCursorPosition = std::max<int>(mCursorPosition - 1,
                                     0);
    } else if (e.keycode == Key::Enter) {
      auto before = mContent.substr(0, mCursorPosition);
      before += '\n';
      before += mContent.substr(mCursorPosition, mContent.size());
      mContent = std::move(before); 
      mCursorPosition += 1;
    }
  } else if (e.type == InputType::ScrollVertical) {
    mViewportY += e.velocityY;
    mViewportY = std::clamp(mViewportY, 0, mMaxViewportY);
  } else if (e.type == InputType::MouseClick) {
    SetFocusedView(this);
  }
}

