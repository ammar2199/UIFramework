#include "ScrollableView.h"

ScrollableView::ScrollableView(WindowRoot* const window) 
                  : View(window), mSpacing(2), 
                  mOrientation(ScrollableView::Orientation::Vertical), 
                  mViewportX(0), mViewportY(0) {
}

void ScrollableView::AddView(View* view) {
  mChildren.push_back(view);
}

void ScrollableView::measure() {
  for (auto childView : mChildren) {
    childView->measure();
  }
}

void ScrollableView::UpdateConstraints() {
  for (auto childView : mChildren) {
    childView->UpdateConstraints();
  }
}

void ScrollableView::layout(int l, int t, int r, int b) {
  View::layout(l, t, r, b);
  
  // Place Children in their respective framebuffer coordinates.
  //  int lChild = mLeft - mViewportX, tChild = mTop - mViewportY;
  int lowestPoint = std::numeric_limits<int>::min();
  for (auto childView : mChildren) {
    int lChild = GetTableau().GetResult(childView->GetLeftVar()) - mViewportX;
    int tChild = GetTableau().GetResult(childView->GetTopVar()) - mViewportY;
    int rChild = GetTableau().GetResult(childView->GetRightVar()) - mViewportX;
    int bChild = GetTableau().GetResult(childView->GetBottomVar()) - mViewportY;
    childView->layout(lChild, tChild, rChild, bChild);
    lowestPoint = std::max<int>(lowestPoint, bChild + mViewportY);
  }
  mMaxViewportY = std::max<int>(lowestPoint - mBottom, 0);
}

void ScrollableView::draw() {
  // background
  GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
  GetGraphics()->DrawRect(mLeft, mTop, mRight, mBottom);
  
  // outline
  GetGraphics()->SetColor(mOutlineRGB[0], mOutlineRGB[1], 
                                   mOutlineRGB[2], 1.f);
  GetGraphics()->DrawOutline(mLeft, mTop, mRight, mBottom);
  
  Rect originalScissor;
  GetGraphics()->SetScissor(GetLeft(), GetTop(), 
                                     GetWidth(), GetHeight(), originalScissor);
  // Draw Children
  for (auto childView : mChildren) {
    if (childView->IntersectsView(this)) {
      childView->draw();  
    }
  }

  Rect r;
  GetGraphics()->UnsetScissor();
  GetGraphics()->SetScissor(originalScissor, r); // Reset to original.
}

// XXX: May need to be adjusted more.
void ScrollableView::InjectInputEvent(const InputEvent& e) {
  if (e.type == InputType::ScrollVertical) {
    mViewportY += e.velocityY;
    mViewportY = std::clamp<int>(mViewportY, 0, mMaxViewportY);
  } else if (e.type == InputType::MouseClick) {
    for (auto child : mChildren) {
      if (child->HitBy(e)) {
        child->InjectInputEvent(e);
      }
    }
  }
}


