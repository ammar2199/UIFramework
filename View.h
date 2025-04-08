#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

#include "Graphics2D.h"
#include "Expression.h"
#include "Timer.h"

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

// TODO: Solution for memory-management/resource leaks.

class View;
class WindowRoot;

enum class InputType {
  MouseClick,
  MouseHeld,
  ScrollVertical,
  ScrollHorizontal,
  Hover,
  Text,
  Key,
};

enum class Key {
  Backspace, 
  LeftArrow,
  RightArrow,
  Enter, // NewLine: '\n'.
};

struct InputEvent {
  InputType type;
  int pointerX;
  int pointerY;
  char character;
  Key keycode;
  int velocityX; // XXX: Rename this to 'magnitude' or something.
  int velocityY;
};

class Box {
  friend class WindowRoot;
 public:
   // Constructors
   Box() : mLeftVar(GenerateVarName(BoxAttribute::Left)), 
           mRightVar(GenerateVarName(BoxAttribute::Right)), 
            mTopVar(GenerateVarName(BoxAttribute::Top)), 
            mBottomVar(GenerateVarName(BoxAttribute::Bottom)),
            mWidthVar(GenerateVarName(BoxAttribute::Width)),
            mHeightVar(GenerateVarName(BoxAttribute::Height)),
            mHorizontalConstraint(nullptr), mVerticalConstraint(nullptr),
            mLeftConstraint(nullptr), mRightConstraint(nullptr),
            mTopConstraint(nullptr), mBottomConstraint(nullptr),
            mWidthConstraint(nullptr), mHeightConstraint(nullptr) {
   }

   Box(const Box& b) = delete;

   // Destructor
   ~Box() = default;  
   
   // Operators
   Box& operator=(Box& b) = delete;
   Box& operator=(Box&& b) = delete;

   // Getters
   Variable GetLeftVar() const { return mLeftVar; }
   Variable GetRightVar() const { return mRightVar; }
   Variable GetTopVar() const { return mTopVar; }
   Variable GetBottomVar() const { return mBottomVar; }
   Variable GetWidthVar() const { return mWidthVar; }
   Variable GetHeightVar() const { return mHeightVar; }

   std::string GenerateVarName(BoxAttribute attribute) const;

 protected:
   void SetHorizontalConstraint(Constraint* const c) { mHorizontalConstraint = c; }
   void SetVerticalConstraint(Constraint* const c) { mVerticalConstraint = c; }
   void SetLeftConstraint(Constraint* const c) { mLeftConstraint = c; }
   void SetRightConstraint(Constraint* const c) { mRightConstraint = c; }
   void SetTopConstraint(Constraint* const c) { mTopConstraint = c; }
   void SetBottomConstraint(Constraint* const c) { mBottomConstraint = c; }
   void SetWidthConstraint(Constraint* const c) { mWidthConstraint = c; }
   void SetHeightConstraint(Constraint* const c) { mHeightConstraint = c; }
  
   Constraint* GetHorizontalConstraint() const { return mHorizontalConstraint; }
   Constraint* GetVerticalConstraint() const { return mVerticalConstraint; }
   Constraint* GetLeftConstraint() const { return mLeftConstraint; }
   Constraint* GetRightConstraint() const { return mRightConstraint; }
   Constraint* GetTopConstraint() const { return mTopConstraint; }
   Constraint* GetBottomConstraint() const { return mBottomConstraint; }
   Constraint* GetWidthConstraint() const { return mWidthConstraint; }
   Constraint* GetHeightConstraint() const { return mHeightConstraint; }
  
   // XXX: I don't think this is used any longer...
   // Box Constraints. Two:
   //   1. Box-Width = Box-RightVar - Box-LeftVar
   //   2. Box-Height = Box-BottomVar - Box-TopVar    
   //
   //   XXX/TODO: How should I do this? Since it's currently not possible to
   //             represent these Constraints with our 'class Constraint'. 
   //             Since it only takes stores Constraints with two variables.
   Constraint* GetBoxXConstraint() const {
    if (mBoxXConstraint) { return mBoxXConstraint; }
    //mBoxXConstraint = new Constraint(this, BoxAttribute::Width, Relation::EqualTo, this, );
   }

   Constraint* GetBoxYConstraint() const {
     if (mBoxYConstraint) { return mBoxYConstraint; }
   }

 private:

  Variable mLeftVar;
  Variable mRightVar;
  Variable mTopVar;
  Variable mBottomVar;
  Variable mWidthVar;
  Variable mHeightVar;
  
  Constraint* mHorizontalConstraint;
  Constraint* mVerticalConstraint;
  
  // Useful in situations where we force a box to be of certain size.
  Constraint* mLeftConstraint; // Left = n
  Constraint* mRightConstraint; // Right = m
  Constraint* mTopConstraint;
  Constraint* mBottomConstraint;
  Constraint* mWidthConstraint;
  Constraint* mHeightConstraint;
  Constraint* mBoxXConstraint; // Box-Width = Box-RightVar - Box-LeftVar
  Constraint* mBoxYConstraint; // Box-Height = Box-BottomVar - Box-TopVar    
};

// Polymorphic Base class for all Views
class View : public Box {
 public:
  View(WindowRoot* const window);
  View() = delete;
  View(const View& v) = delete;
  
  // Destructor
  ~View() = default;

  // operators
  View& operator=(const View& view) = delete; // assignment
  
  virtual void measure() = 0;  // Measures all descendants and current views size, if necessary.
  
  // In Widgets like a TextView, this would involve us having to  
  // Construct Intrisic Size Constraints.
  // Update Constraints based on measured input, if necessary.
  virtual void UpdateConstraints() {}
  
  // Set View's position and children's view position.
  virtual void layout(int l, int t, int r, int b) {
    mLeft = l, mTop = t, mRight = r, mBottom = b;
    mWidth = mRight - mLeft;
    mHeight = mBottom - mTop;
  }

  virtual void draw() = 0; // Draw Your Own View, then all Children View.
   // TODO : Make this non-const. Why? Views with children
  //      may want to modify how it forwards input events.
  virtual void InjectInputEvent(const InputEvent& e);
  
  int GetWidth() const { return mWidth; }
  int GetHeight() const { return mHeight; }
  int GetTop() const { return mTop; }
  int GetLeft() const { return mLeft; }
  int GetRight() const { return mRight; }
  int GetBottom() const { return mBottom; }
  int GetContentWidth() const { return mContentWidth; }
  int GetContentHeight() const { return mContentHeight; }
  bool HasFocus() const { return mHasFocus; }
  void RegisterClickEvent(std::function<void(View*)> onClick);

  void SetRGB(float r, float g, float b) {
    mRGB[0] = r;
    mRGB[1] = g;
    mRGB[2] = b;
  }

  void SetBackground(float r, float g, float b) {
    SetRGB(r, g, b);
  }

  void SetOutlineRGB(float r, float g, float b) {
    mOutlineRGB[0] = r;
    mOutlineRGB[1] = g;
    mOutlineRGB[2] = b;
  }
  
  // Returns True iff InputEvent's pointer position
  // is within the Views bounds.
  bool HitBy(const InputEvent& e) const;
  
  // Returns True iff both Views bounds intersect
  bool IntersectsView(const View* const view) const;

 
  bool IsFocusedView() const;
 protected:
  Tableau2& GetTableau();
  void AddHeldView(View* const view);

  void SetFocusedView(View* const view);

  Graphics2D* GetGraphics();
  
  WindowRoot* mWindow;
 
  int mContentWidth; // Set in measure()
  int mContentHeight;
  int mWidth; // Set in layout()
  int mHeight;
 
  int mPrevWidth; // Contains previously drawn width from last frame
  int mPrevHeight; // Contains previosly drawn height from last frame
                   // XXX: When should it be set?

  int mLeft; // Set in layout()
  int mTop;
  int mBottom;
  int mRight;

  std::array<float, 3> mRGB; // Background of View
  std::array<float, 3> mOutlineRGB; // Outline of View
  
 bool mHasFocus = false;
 std::function<void(View*)> mOnClickEvent;
};

class WindowRoot : public Box {
 public:
   friend class View;

   WindowRoot(int w, int h);
   WindowRoot() = delete;
   WindowRoot(const WindowRoot&) = delete;

   ~WindowRoot();

   WindowRoot& operator=(const WindowRoot&) = delete;
   WindowRoot& operator=(WindowRoot&&) = delete;
   
   bool Init(const char** extensions, const uint32_t extensionCount);
   
   VkInstance GetVkInstance() {
     return GetGraphics()->GetVkInstance();
   }
   
   void SetSurface(VkSurfaceKHR surface);
   
   void AddView(View* const v) { 
     mViews.push_back(v); 
   }

   void AddConstraint(Constraint* const c) { 
     mTableau.AddConstraint(c);
   }

  int GetWidth() const { return mWidth; }

  int GetHeight() const { return mHeight; }

  void SetRGB(float r, float g, float b) {
    mRGB[0] = r, mRGB[1] = g, mRGB[2] = b;
  }

   // Sets window size for upcoming frame.
   // Will recreate swapchain if new width,height
   // differs from previous frames.
   void Resize(const int newWidth, const int newHeight);
  
   // Performs measure, layout, and draw pass.
   // Results in Command Buffer ready 
   // to be drawn then presented.
   void UpdateViewHierarchy();
    
   // Submits Command Buffer for Rendering
   // and then Presentation.
   void Present();

   void InjectInputEvent(const InputEvent& e);
   
   void AddTimer(Timer* timer) {
     mTimers.insert(timer);
   }

  void RemoveTimer(Timer* timer) {
     mTimers.erase(timer);
  }

  double GetTimeout() const;

 // To synchronize every view to same Time
  void SetCurrentTime( 
     std::chrono::time_point<std::chrono::high_resolution_clock> tp);

  std::chrono::time_point<std::chrono::high_resolution_clock> GetCurrentTime();


 private:
   // Constructs Window Constraints.
   void GenerateConstraints();
    
   // Updates Constraints... for some reason.
   void UpdateConstraints();
    
   // These are for Views to use.
   const View* GetFocusedView() const;
   Tableau2& GetTableau() {
     return mTableau;
   }

   void AddHeldView(View* const view) {
      mHeldViews.push_back(view);
   }

  void SetFocusedView(View* const view) {
      mFocusedView = view;
  }

  Graphics2D* GetGraphics() {
       return mGraphics.get();
  }

   std::vector<View*> mViews;
   int mWidth;
   int mHeight;
   std::array<float, 3> mRGB;
   
   // Reference to Graphics Resources.
   std::unique_ptr<Graphics2D> mGraphics;

   Tableau2 mTableau;

   std::unordered_set<Timer*> mTimers; // Note: When timer is destroyed
                                       //       it should tell Window
                                       //       to remove itself.
   std::chrono::time_point<std::chrono::high_resolution_clock> mWindowTime;
   
   bool mInit;
    
   // For Views which are being dragged around.
   std::vector<View*> mHeldViews;
    
   View* mFocusedView;
};

