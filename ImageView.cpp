#include "ImageView.h"

ImageView::ImageView(WindowRoot* window, const std::string& uri, 
                     bool scrollable) : View(window),
                     mURI(uri), mScrollable(scrollable), 
                     mImageResource(-1) {
  mImageResource = GetGraphics()->AddImage(mURI);
}

// TODO: When to perform scaling?
//       If we perform it in draw, then layout will be incorrect no?
void ImageView::measure() {
  ImageInfo imgInfo = GetGraphics()->GetImageInfo(mImageResource);
  mContentWidth = imgInfo.width;
  mContentHeight = imgInfo.height;
}

void ImageView::UpdateConstraints() {
 UpdateWidthConstraint();
 UpdateHeightConstraint();
}

void ImageView::UpdateWidthConstraint() {
  if (!mAddedWidthConstraint) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Width, 
                          Relation::EqualTo, nullptr, 
                          BoxAttribute::NoAttribute, 
                          0.f, mContentWidth, 1)); 
    mAddedWidthConstraint = true;
  }
}

void ImageView::UpdateHeightConstraint() {
  if (!mAddedHeightConstraint) {
    mWindow->AddConstraint(new Constraint(this, BoxAttribute::Height, 
                           Relation::EqualTo, nullptr, 
                           BoxAttribute::NoAttribute, 
                           0.f, mContentHeight, 1)); 
    mAddedHeightConstraint = true;
  }
}

void ImageView::draw() {
  GetGraphics()->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
  GetGraphics()->DrawRect(GetLeft(), GetTop(),
                          GetRight(), GetBottom());
  GetGraphics()->SetColor(mOutlineRGB[0], mOutlineRGB[1], 
                          mOutlineRGB[2], 1.f);
  GetGraphics()->DrawOutline(GetLeft(), GetTop(),
                             GetRight(), GetBottom());

  // TODO: Need to fix when Width/Height > mContentWidth, mContentHeight.
  //       Check for both Scrollable and Non-Scrollable.
  if (mScrollable) {
    // Only show the part of Image that fits in ImageView.
    std::vector<vec2> textureCoords;
    vec2 topLeft{static_cast<float>(mContentOffsetX) / mContentWidth, static_cast<float>(mContentOffsetY) / mContentHeight};
    vec2 topRight{static_cast<float>(mContentOffsetX + GetWidth()) / mContentWidth, static_cast<float>(mContentOffsetY) / mContentHeight};
    vec2 bottomRight{static_cast<float>(mContentOffsetX + GetWidth()) / mContentWidth, static_cast<float>(mContentOffsetY + GetHeight()) / mContentHeight};
    vec2 bottomLeft{static_cast<float>(mContentOffsetX) / mContentWidth, static_cast<float>(mContentOffsetY + GetHeight()) / mContentHeight};
    textureCoords.push_back(topLeft);
    textureCoords.push_back(topRight);
    textureCoords.push_back(bottomRight);
    textureCoords.push_back(bottomLeft);
    GetGraphics()->DrawTexturedRect(mImageResource, GetLeft(), 
                                    GetTop(), GetRight(), 
                                    GetBottom(), textureCoords);
   } else {
     // Scale the Image to fit inside ImageView.
     // Maintain Image's aspect ratio.
     int l = GetLeft(), r = GetRight(), t = GetTop(), b = GetBottom();
     if (mContentWidth > GetWidth() || mContentHeight > GetHeight()) {
       double scale = std::min<double>(static_cast<double>(GetWidth())/mContentWidth,
                                       static_cast<double>(GetHeight())/mContentHeight);
       double newWidth = mContentWidth * scale;
       double newHeight = mContentHeight * scale;
       l = ((GetLeft() + GetRight()) / 2.0) - (newWidth/2.0);
       r = ((GetLeft() + GetRight()) / 2.0) + (newWidth/2.0);
       t = ((GetTop() + GetBottom()) / 2.0) - (newHeight/2.0);
       b = ((GetTop() + GetBottom()) / 2.0) + (newHeight/2.0);
     }
     GetGraphics()->DrawTexturedRect(mImageResource, l, t, r, b);
   }
}

void ImageView::InjectInputEvent(const InputEvent& e) {
  switch (e.type) {
    case InputType::ScrollVertical:
      if (mScrollable && mContentHeight > GetHeight()) {
        mContentOffsetY += e.velocityY;
        mContentOffsetY = std::clamp<int>(mContentOffsetY, 0, mContentHeight - GetHeight());
      }
      break;
    case InputType::ScrollHorizontal:
      if (mScrollable && mContentWidth > GetWidth()) {
        mContentOffsetX += e.velocityX;
        mContentOffsetX = std::clamp<int>(mContentOffsetX, 0, mContentWidth - GetWidth());
      }
      break;
     default:
      View::InjectInputEvent(e);
      break;
  }
}

