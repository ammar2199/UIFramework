#pragma once

#include "View.h"

// TODO: This doesn't work too cleanly when the entire Window is Resized.
//       Since mConstant isn't changed.
class GuidelineView : public View {
 public:
   enum class Orientation {
     Vertical,
     Horizontal
   };

   GuidelineView(WindowRoot* window, double constant, double size = 4, 
                GuidelineView::Orientation orientation = GuidelineView::Orientation::Vertical);

   virtual void measure() override;
   virtual void draw() override;
   void InjectInputEvent(const InputEvent& e) override;

 private:
   double mConstant; // Represents either CenterX if Orientation::Vertical
                     // or CenterY if Orientation::Horizontal
   double mSize; // Represents width or height of GuidelineView box.
   const Orientation mOrientation;
   int mLastPosition;
};

