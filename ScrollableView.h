#pragma once

#include "View.h"

#include <vector>

// Note: This widget merely uses the Tableau to place
//       it's children, hence it's still up to the constraints
//       upon those children to achieve desired layout.
class ScrollableView : public View {
  public:
    enum class Orientation {
      Vertical,
      Horizontal // TODO: Not implemented.
    };

    ScrollableView(WindowRoot* const window);
    virtual ~ScrollableView() = default;

    void AddView(View* const v);

    virtual void measure() override; // Measures children. Does not measure self size. Since 
                                     // height, and width are dependent upon tableau.
    
    virtual void UpdateConstraints() override; // Allows children to generate
                                               // Constraints.
    virtual void layout(int l, int t, int r, int b) override; // Places children
                                                              // in FB coordinates
                                                              // using Tableau.
    virtual void draw() override;

    virtual void InjectInputEvent(const InputEvent& e) override;
    
  private:
    std::vector<View*> mChildren;
    uint32_t mSpacing;
    Orientation mOrientation; // TODO: width and height should be automatically set to widest
                              //       child. If only one width/height is set for this widget.
    int mViewportX;
    int mViewportY;
    int mMaxViewportX;
    int mMaxViewportY;
};
