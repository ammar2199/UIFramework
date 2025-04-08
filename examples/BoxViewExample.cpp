#include <array>
#include <chrono>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "Setup.h"

#include "../BoxView.h"

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("BoxView Example");
  SetupEvents(GlfwResources.window);

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  
  WindowRoot* const windowRoot = new WindowRoot(currentWidth, currentHeight);
  if (!windowRoot || !windowRoot->Init(glfwExtensions, glfwExtensionCount)) {
    return 1; // E_INIT
  }
  /* Create Vulkan Surface using GLFW */
  VkSurfaceKHR surface; 
  VkInstance i = windowRoot->GetVkInstance();
  glfwCreateWindowSurface(i, GlfwResources.window, nullptr, &surface);
  windowRoot->SetSurface(surface);

  BoxView* boxView = new BoxView(windowRoot);
  Constraint* topConstraint = new Constraint(boxView, BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* leftConstraint = new Constraint(boxView, BoxAttribute::Left, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
 
  boxView->SetRGB(0.5f, 0.5f, 0.75f);
  windowRoot->AddView(boxView);
  windowRoot->AddConstraint(topConstraint);
  windowRoot->AddConstraint(leftConstraint);

  BoxView* boxView2 = new BoxView(windowRoot);
  boxView2->SetRGB(0.75f, 0.5f, 0.5f);
  Constraint* topConstraint2 = new Constraint(boxView2, BoxAttribute::Top, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Bottom, 0.5f, 0.f);

  Constraint* bottomConstraint2 = new Constraint(boxView2, BoxAttribute::Bottom, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Bottom, 0.8f, 0.f);
 
  Constraint* leftConstraint2 = new Constraint(boxView2, BoxAttribute::Left, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Left, 1.f, 50.f);

  constexpr int Priority = 2;
  Constraint* rightConstraint2 = new Constraint(boxView2, BoxAttribute::Right, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Right, 1.f, -100.f, Priority);

  Constraint* widthConstraint2 = new Constraint(boxView2, BoxAttribute::Width, 
                                    Relation::LessThanOrEqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 800, Priority + 1);
 
  windowRoot->AddView(boxView2);
  windowRoot->AddConstraint(topConstraint2);
  windowRoot->AddConstraint(bottomConstraint2);
  windowRoot->AddConstraint(leftConstraint2);
  windowRoot->AddConstraint(rightConstraint2);
  windowRoot->AddConstraint(widthConstraint2);

  while (!glfwWindowShouldClose(GlfwResources.window)) {
    std::array<InputEvent, 5> mInputEvents; // One for Mouse, Scroll, Text, and Key Input
    // Set Time
    auto currentTimePoint = std::chrono::high_resolution_clock::now();
    windowRoot->SetCurrentTime(currentTimePoint);
    if (ScrollEvent) {
      ScrollEvent = false;
      mInputEvents[0].type = InputType::ScrollVertical;
      mInputEvents[0].pointerX = MouseXPos; 
      mInputEvents[0].pointerY = MouseYPos;
      mInputEvents[0].velocityX = ScrollXOffset * SCROLL_FACTOR;
      mInputEvents[0].velocityY = ScrollYOffset * SCROLL_FACTOR;
      windowRoot->InjectInputEvent(mInputEvents[0]);
    } else if (TextEvent) {
      TextEvent = false;
      mInputEvents[1].type = InputType::Text;
      mInputEvents[1].character = static_cast<char>(characterPressed);
      windowRoot->InjectInputEvent(mInputEvents[1]);
    } else if (KeyEvent) {
      KeyEvent = false;
      mInputEvents[2].type = InputType::Key;
      mInputEvents[2].keycode = pressedKey;
      windowRoot->InjectInputEvent(mInputEvents[2]);
    } else if (MouseClickEvent) {
      MouseClickEvent = false;
      mInputEvents[3].type = InputType::MouseClick;
      mInputEvents[3].pointerX = MouseXPos;
      mInputEvents[3].pointerY = MouseYPos;
      windowRoot->InjectInputEvent(mInputEvents[3]);   
    } else if (MouseHeldEvent) {
      MouseHeldEvent = false;
      mInputEvents[4].type = InputType::MouseHeld;
      mInputEvents[4].pointerX = MouseXPos;
      mInputEvents[4].pointerY = MouseYPos;
      windowRoot->InjectInputEvent(mInputEvents[4]);
    }

    windowRoot->Resize(currentWidth, currentHeight);
    windowRoot->UpdateViewHierarchy();
    windowRoot->Present();
    double timeout = windowRoot->GetTimeout();
    glfwWaitEventsTimeout(timeout); // Blocks until event happens or timeout reached.
                                 // Timeout ensures window get's redrawn by x-seconds,
                                 // which is important for anything animated.
  };
  delete windowRoot;
  glfwDestroyWindow(GlfwResources.window);
  glfwTerminate(); 
  
  return 0;
}
