#pragma once

#include "View.h"
#include "Font.h"
#include "Timer.h"

#include <string>
#include <array>
#include <vector>

// TODO: In general, shouldn't I make the members protected?
//       so that children can access?
class EditTextView : public View {
  public:
  EditTextView(WindowRoot* const window, 
               const std::string& typefaceFile, 
               int fontSize=25);
  
  virtual ~EditTextView() = default;

  virtual void measure() override;
  virtual void layout(int l, int t, int r, int b) override;
  virtual void draw() override;
  virtual void InjectInputEvent(const InputEvent& e) override;
  
  void SetTextRGB(float r, float g, float b);
  void SetText(const std::string& str);
  void SetTextSize(int pointSize);

  void SetCursorRGB(float r, float g, float b);

  private: 
  std::string mTypeface;
  std::string mContent;
  int mFontSize;
  int mLineSpace;
  int mCursorPosition;
  int mLoaded;
  FontId_t mFontResources{-1};
  bool mHorizontalScroll;
  std::vector<std::string> mLineContent;
  std::array<float, 3> mTextRGB;
  std::array<float, 3> mCursorRGB;

  int mViewportY;
  int mMaxViewportY;
  
  // Cursor Line and Baseline X Position.
  // Determined during EditTextView::layout()
  int mCursorLine;
  int mCursorBaselineX;
  int mCursorWidth;
  int mCursorHeight;
  // Glyph2 cursorGlyph // <--- This would be a great way to
  // store widht, height, horizontal-advance, 
  // and and be able to use an Image if we wanted one!

  // Blinking Cursor
  // 1 second off, 0.25 seconds on.
  Timer mCursorTimer;

  int mIntervalsPassed{0};
};
