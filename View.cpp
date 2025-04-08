#include "View.h"

#include <cassert>
#include <string>
#include <sstream>

#include <vulkan/vulkan.h>

#include "thirdparty/stb_image.h"

std::string Box::GenerateVarName(BoxAttribute attribute) const {
  std::stringstream stream;
  stream << "Box(" << this << ").";
  switch (attribute) {
    case BoxAttribute::Left:
      stream << "Left";      
      break;
    case BoxAttribute::Right:
      stream << "Right";
      break;
    case BoxAttribute::Top:
      stream << "Top";
      break;
    case BoxAttribute::Bottom:
      stream << "Bottom";
      break;
    case BoxAttribute::Width:
      stream << "Width";
      break;
    case BoxAttribute::Height:
      stream << "Height";
      break;
    default:
      stream << "NoAttribute";
      break;
  };
  return stream.str();
}

View::View(WindowRoot* const window) : mWindow(window), 
                                      mContentWidth(0),
                                      mContentHeight(0),
                                      mWidth(0), mHeight(0),
                                      mPrevWidth(0), mPrevHeight(0),
                                      mLeft(0), mRight(0),
                                      mTop(0), mBottom(0),
                                      mRGB({0.f, 0.f, 0.f}),
                                      mOutlineRGB({0.f, 0.f, 0.f}) {
  assert(mWindow && "WindowRoot must be Non-Nullptr to properly construct View object");
  
  // System-defined Box Constraints
  window->GetTableau().AddConstraint(GetWidthVar(), Relation::EqualTo, GetRightVar() - GetLeftVar());
  window->GetTableau().AddConstraint(GetHeightVar(), Relation::EqualTo, GetBottomVar() - GetTopVar());
}

void View::InjectInputEvent(const InputEvent& e) {
  if (e.type == InputType::MouseClick) {
    if (mOnClickEvent) {
      mOnClickEvent(this);
    }
  }
}

void View::RegisterClickEvent(std::function<void(View*)> onClick) {
  mOnClickEvent = onClick;
}

bool View::HitBy(const InputEvent& e) const {
  return e.pointerX > mLeft && e.pointerX < mRight && 
         e.pointerY > mTop && e.pointerY < mBottom;
}

bool View::IntersectsView(const View* const view) const {
  bool xOverlaps = (mRight > view->GetLeft() && mLeft < view->GetRight());
  bool yOverlaps = (mBottom > view->GetTop() && mTop < view->GetBottom());
  return xOverlaps && yOverlaps;
}

bool View::IsFocusedView() const {
  return mWindow->GetFocusedView() == this;
}

Tableau2& View::GetTableau() {
  return mWindow->GetTableau();
}

void View::AddHeldView(View* const view) {
  mWindow->AddHeldView(view);
}

void View::SetFocusedView(View* const view) {
  mWindow->SetFocusedView(view);
}

Graphics2D* View::GetGraphics() {
  return mWindow->GetGraphics();
}

WindowRoot::WindowRoot(int width, 
                       int height) : mWidth(width), 
                       mHeight(height),
                       mGraphics(nullptr), mInit(false),
                       mFocusedView(nullptr),
                       mRGB({0.0f, 0.0f, 0.0f}) {
  //mGraphics = new Graphics2D();
  mGraphics = std::make_unique<Graphics2D>();
}

WindowRoot::~WindowRoot() {
  // TODO: Delete View Hierarchy too?
  //       std::shared_ptrs?
}

bool WindowRoot::Init(const char** extensions, const uint32_t extensionCount) {
     if (mInit) { return true; }
     // Initialize our Graphics Engine.
     bool retVal = mGraphics->Init(extensions, extensionCount);
     if (!retVal) { return retVal; }
     GenerateConstraints();
     mTableau.AddConstraint(GetWidthConstraint());
     mTableau.AddConstraint(GetHeightConstraint());
     mTableau.AddConstraint(GetLeftConstraint());
     mTableau.AddConstraint(GetRightConstraint());
     mTableau.AddConstraint(GetTopConstraint());
     mTableau.AddConstraint(GetBottomConstraint());

     // Generated Box Constraints
     mTableau.AddConstraint(GetWidthVar(), Relation::EqualTo, GetRightVar() - GetLeftVar());
     mTableau.AddConstraint(GetHeightVar(), Relation::EqualTo, GetBottomVar() - GetTopVar());
     mInit = true;
     return retVal;
}

void WindowRoot::GenerateConstraints() {
     // Note: Must be non-required since Edit vars must be non-required.
     // But we also don't want these to be overpowered by some 
     // other constraint.
     constexpr int Strength =  static_cast<int>(ConstraintStrength::REQUIRED) - 1;
     SetWidthConstraint(new Constraint(this, BoxAttribute::Width, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, mWidth-1, Strength));
     SetHeightConstraint(new Constraint(this, BoxAttribute::Height, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, mHeight-1, Strength));
     SetLeftConstraint(new Constraint(this, BoxAttribute::Left, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, 0.0, Strength));
     SetRightConstraint(new Constraint(this, BoxAttribute::Right, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, mWidth-1, Strength));
     SetTopConstraint(new Constraint(this, BoxAttribute::Top, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, 0.0, Strength));
     SetBottomConstraint(new Constraint(this, BoxAttribute::Bottom, Relation::EqualTo, nullptr, BoxAttribute::NoAttribute, 0.0, mHeight-1, Strength));
}

void WindowRoot::SetSurface(VkSurfaceKHR surface) {
  mGraphics->SetSurface(surface);
}


void WindowRoot::UpdateViewHierarchy() {
  // Measure pass
  for (auto* view : mViews) {
    view->measure();
    view->UpdateConstraints();
  }

  // XXX: why is this here?
  // TODO
  UpdateConstraints(); // Update the window constraints.
    
  mTableau.Solve(); // Tableau Solve

  // Layout
  for (auto* view : mViews) {
    const int left = static_cast<int>(mTableau.GetResult(view->GetLeftVar()));
    const int top = static_cast<int>(mTableau.GetResult(view->GetTopVar()));
    const int width = static_cast<int>(mTableau.GetResult(view->GetWidthVar()));
    const int height = static_cast<int>(mTableau.GetResult(view->GetHeightVar()));
    view->layout(left, top, left + width, top + height);
  }
    
  
  // Draw
  mGraphics->BeginRecording(); // Note: Blocks until command buffer is available
   
  mGraphics->SetColor(mRGB[0], mRGB[1], mRGB[2], 1.f);
  mGraphics->DrawRect(0, 0, GetWidth(), GetHeight());

  // This is needed so that when we Resize window,
  // we get rid of the scissor applied by the View 
  // rendered last. (which should be the old Window size).
  mGraphics->UnsetScissor();
  Rect blah;
  mGraphics->SetScissor(0, 0, GetWidth(), GetHeight(), blah);
  for (auto* view : mViews) {
   view->draw(); // Draw all descendant views.
  }
  mGraphics->EndRecording(); // End renderpass. Stops command buffer Recording
}

// TODO: Is this necessary?
//       Frankly, no, it seems irrelvant. almost. Constraint
//       isn't really considered by the tableau.
//       Tableau class de-analyzes it and works with
//       expressions.
void WindowRoot::UpdateConstraints() {
  GetWidthConstraint()->UpdateConstant(mWidth);     
  GetRightConstraint()->UpdateConstant(mWidth);     
  GetBottomConstraint()->UpdateConstant(mHeight);     
  GetHeightConstraint()->UpdateConstant(mHeight);     
}

const View* WindowRoot::GetFocusedView() const {
  return mFocusedView;
}

void WindowRoot::Present() {
  mGraphics->Present();
}

void WindowRoot::Resize(int newWidth, int newHeight) {
  if (mWidth != newWidth || mHeight != newHeight) {
    GetGraphics()->Resize(); // Will recreate swapchain.
  }
 
  mWidth = newWidth;
  mHeight = newHeight;
 
  // Does this really go here or in the UpdateConstraint section ?
  mTableau.UpdateConstraint(*GetWidthConstraint(), mWidth-1);
  mTableau.UpdateConstraint(*GetRightConstraint(), mWidth-1);
  mTableau.UpdateConstraint(*GetHeightConstraint(), mHeight-1);
  mTableau.UpdateConstraint(*GetBottomConstraint(), mHeight-1);
  mTableau.FinishUpdates();
}

void WindowRoot::InjectInputEvent(const InputEvent& e) {
  if (e.type == InputType::MouseHeld && !mHeldViews.empty()) {
    for (auto* heldView : mHeldViews) {
      heldView ->InjectInputEvent(e);
    }
    return;
  }
  mHeldViews.clear();
  
  if (e.type != InputType::MouseClick && mFocusedView) {
   mFocusedView->InjectInputEvent(e);
   return;
  }
  mFocusedView = nullptr;

  for (auto* view : mViews) {
    if (view->HitBy(e)) { //|| view->HasFocus()) {
     view->InjectInputEvent(e);
    }
  }
}

double WindowRoot::GetTimeout() const {
  if (mTimers.empty()) return std::numeric_limits<double>::max();
  double minInterval = 0;
  // XXX: Does this work when we have more than one Timer?
  for (auto timer : mTimers) {
    minInterval = std::min<double>(minInterval, timer->TimeTillNextInterval(mWindowTime));
  }
  return minInterval;
}

// To synchronize every view to same Time
void WindowRoot::SetCurrentTime( 
   std::chrono::time_point<std::chrono::high_resolution_clock> tp) {
  mWindowTime = tp;
}

std::chrono::time_point<std::chrono::high_resolution_clock> WindowRoot::GetCurrentTime() {
  return mWindowTime;
}

