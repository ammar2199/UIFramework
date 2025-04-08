#include "GuidelineView.h"

GuidelineView::GuidelineView(WindowRoot* window, 
                             double constant, double size,
                             GuidelineView::Orientation orientation) : 
                              View(window), mConstant(constant), 
                              mSize(size), mOrientation(orientation) {
  // Create Constraints.
  // Note: Edit Vars must be optional!
  if (mOrientation == Orientation::Vertical) {
    SetTopConstraint(new Constraint(this, BoxAttribute::Top, Relation::EqualTo,
                                    window, BoxAttribute::Top, 1.f, 0.f));
    SetBottomConstraint(new Constraint(this, BoxAttribute::Bottom, Relation::EqualTo,
                                    window, BoxAttribute::Bottom, 1.f, 0.f));
    
    double left = mConstant - (size/2);
    double right = mConstant + (size/2);
    SetLeftConstraint(new Constraint(this, BoxAttribute::Left, Relation::EqualTo,
                                    nullptr, BoxAttribute::NoAttribute, 0.f, left, 4));
    SetRightConstraint(new Constraint(this, BoxAttribute::Right, Relation::EqualTo,
                                    nullptr, BoxAttribute::NoAttribute, 0.f, right, 4));
  } else { // Orientation::Horizontal
    SetLeftConstraint(new Constraint(this, BoxAttribute::Left, Relation::EqualTo,
                                    window, BoxAttribute::Left, 1.f, 0.f));
    SetRightConstraint(new Constraint(this, BoxAttribute::Right, Relation::EqualTo,
                                    window, BoxAttribute::Right, 1.f, 0.f));
    
    double top = mConstant - (size/2);
    double bottom = mConstant + (size/2);
    SetTopConstraint(new Constraint(this, BoxAttribute::Top, Relation::EqualTo,
                                    nullptr, BoxAttribute::NoAttribute, 0.f, top, 4));
    SetBottomConstraint(new Constraint(this, BoxAttribute::Bottom, Relation::EqualTo,
                                    nullptr, BoxAttribute::NoAttribute, 0.f, bottom, 4));
  }
  mWindow->AddConstraint(GetTopConstraint());
  mWindow->AddConstraint(GetBottomConstraint());
  mWindow->AddConstraint(GetLeftConstraint());
  mWindow->AddConstraint(GetRightConstraint());
}

void GuidelineView::measure() {}

void GuidelineView::draw() {}

void GuidelineView::InjectInputEvent(const InputEvent& e) {
  if (e.type == InputType::MouseClick) {
    if (mOrientation == GuidelineView::Orientation::Vertical) {
     mLastPosition = e.pointerX;
    } else { // Orientation::Horizontal
     mLastPosition = e.pointerY;
    }
    AddHeldView(this);
  } else if (e.type == InputType::MouseHeld) {
    // Drag Guideline Over by modifying constraints.
    if (mOrientation == GuidelineView::Orientation::Vertical) {
       double difference = e.pointerX - mLastPosition;
       mConstant = mConstant + difference; // Update mConstant
       mConstant = std::clamp<double>(mConstant, 0, mWindow->GetWidth()); 
       double newLeft = mConstant - (mSize/2);
       double newRight = mConstant + (mSize/2);
       mLastPosition = e.pointerX; // Update mLastPosition
       GetTableau().UpdateConstraint(*GetLeftConstraint(), newLeft);
       GetTableau().UpdateConstraint(*GetRightConstraint(), newRight);
    } else { // Orientation::Horizontal 
      // Similar to the above but we adjust to new Top and Bottom
      double difference = e.pointerY - mLastPosition;
      mConstant = mConstant + difference; // Update mConstant
      mConstant = std::clamp<double>(mConstant, 0, mWindow->GetHeight()); 
      double newTop = mConstant - (mSize/2);
      double newBottom = mConstant + (mSize/2);
      mLastPosition = e.pointerY; // Update mLastPosition
      GetTableau().UpdateConstraint(*GetTopConstraint(), newTop);
      GetTableau().UpdateConstraint(*GetBottomConstraint(), newBottom);
    }
  }
}
