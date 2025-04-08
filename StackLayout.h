#pragma once

#include "Graphics2D.h"
#include "View.h"
#include <cstdint>
#include <vector>

//TODO: THIS NEEDS TO BE UPDATED/REWRITTEN....
class StackLayout : public View {
 public:
    StackLayout(WindowRoot* const window) : View(window), mSpacing(0) {};
    virtual void measure() override {
      int childrenHeight = 0;
      int maxChildrenWidth = -1;
      for (View* const v : mChildren) {
        v->measure();
        maxChildrenWidth = std::max(maxChildrenWidth, v->GetWidth());
        childrenHeight += v->GetHeight();
      }
      mWidth = maxChildrenWidth;
      
      mHeight = childrenHeight;
      mHeight += mSpacing * (mChildren.size() - 1);
    }

    virtual void layout(int l, int t, int r, int b) override {
      // Set mLeft, mTop, mBottom, mRight?
      mLeft = l;
      mTop = t;
      mRight = r;
      mBottom = b;
      
      // Start at position relative to StackLayout position.
      int lChild = l, tChild = t;
      int rChild, bChild;
      for (View* const child : mChildren) {
        rChild = child->GetWidth() + lChild;
        bChild = child->GetHeight() + tChild;
        child->layout(lChild, tChild, rChild, bChild);
        tChild = bChild + mSpacing;
      }
    }

    virtual void draw() override {
      // Draw Background Color      
      mWindow->GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
      mWindow->GetGraphics()->DrawRect(mLeft, mTop, mRight, mBottom);
      // Draw Children
      for (View* const child : mChildren) {
        child->draw();
      }
    }
    
    void AddView(View* v) { mChildren.push_back(v); }

    void SetSpacing(uint32_t s) {
      mSpacing = s;
    }

 private:
  std::vector<View*> mChildren;
  // enum Orientation {VERTICAL, HORIZONTAL}
  uint32_t mSpacing;
};
