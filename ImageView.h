#pragma once

#include "View.h"
#include "Image.h"

#include <string>

// XXX: Note: What to do when ImageView content width and height >=  
//            width, height?
//
//            Right now it's acting weirdly.
//            Maybe I should "stretch" Image, yea, that would be better.
//            TODO...
class ImageView : public View {
 public:
   ImageView(WindowRoot* window, const std::string& uri, bool scrollable = false);
   
   // Loop
   virtual void measure() override;
   virtual void UpdateConstraints() override;
   virtual void draw() override;

   void InjectInputEvent(const InputEvent& e) override;

  private:
   void UpdateWidthConstraint();
   void UpdateHeightConstraint();
    
    std::string mURI;
    bool mScrollable;
    uint32_t mContentOffsetX{0}; // Scroll Offset X 
    uint32_t mContentOffsetY{0}; // Scroll Offset Y
    ImageId_t mImageResource;
    bool mAddedHeightConstraint{false};
    bool mAddedWidthConstraint{false};
};

