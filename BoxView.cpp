#include "BoxView.h"

BoxView::BoxView(WindowRoot* const window) : View(window) {
}

void BoxView::measure() {
  mContentWidth = 175; // Arbitrarily chosen.
  mContentHeight = 75;
}

void BoxView::UpdateConstraints() {
  constexpr int ContentConstraintPriority = 1;
  if (!mMeasuredWidthSet) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Width,
                            Relation::EqualTo, nullptr,
                            BoxAttribute::NoAttribute, 0.f, mContentWidth,
                            ContentConstraintPriority));
    mMeasuredWidthSet = true;
  }
  if (!mMeasuredHeightSet) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Height,
                            Relation::EqualTo, nullptr,
                            BoxAttribute::NoAttribute, 0.f, mContentHeight,
                            ContentConstraintPriority));
    mMeasuredHeightSet = true;
  }
}

void BoxView::draw() {
  GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f); 
  GetGraphics()->DrawRect(GetLeft(), GetTop(), GetRight(), GetBottom());
  GetGraphics()->SetColor(mOutlineRGB[0], mOutlineRGB[1], mOutlineRGB[2], 1.f); 
  GetGraphics()->DrawOutline(GetLeft(), GetTop(), GetRight(), GetBottom());
}
