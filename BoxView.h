#pragma once

#include "View.h"

class BoxView : public View {
  public:
    BoxView(WindowRoot* const window);

    virtual void measure() override;
    virtual void UpdateConstraints() override;
    virtual void draw() override;
  private:
    bool mMeasuredWidthSet{false};
    bool mMeasuredHeightSet{false};
};

