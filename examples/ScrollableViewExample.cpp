#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <limits>
#include <random>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "Setup.h"

#include "../ScrollableView.h"
#include "../TextView.h"

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("ScrollableView Example");
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

  ScrollableView* scrollView = new ScrollableView(windowRoot);
  scrollView->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topScrollConstraint = new Constraint(scrollView, BoxAttribute::Top,
                                                  Relation::EqualTo,
                                                  windowRoot,
                                                  BoxAttribute::Top,
                                                  1.f, 100.f);
 Constraint* bottomScrollConstraint = new Constraint(scrollView, BoxAttribute::Bottom,
                                                  Relation::EqualTo,
                                                  windowRoot,
                                                  BoxAttribute::Bottom,
                                                  1.f, -100.f);
 Constraint* leftScrollConstraint = new Constraint(scrollView, 
                                                   BoxAttribute::Left,
                                                  Relation::EqualTo,
                                                  windowRoot,
                                                  BoxAttribute::Left,
                                                  1.f, 50.f);
  Constraint* widthScrollConstraint = new Constraint(scrollView,
                                                  BoxAttribute::Right,
                                                  Relation::EqualTo,
                                                  windowRoot,
                                                  BoxAttribute::Right,
                                                  1.f, -50.f);
 
  windowRoot->AddView(scrollView);
  windowRoot->AddConstraint(topScrollConstraint);
  windowRoot->AddConstraint(bottomScrollConstraint);
  windowRoot->AddConstraint(leftScrollConstraint);
  windowRoot->AddConstraint(widthScrollConstraint);

  constexpr int NumTextViews = 20; 
  TextView* prevTextView;
  std::random_device randomdev;
  std::mt19937 generator(randomdev());
  std::uniform_real_distribution<double> uniform(0, 1);
  for (int i=0; i<NumTextViews; i++) {
    std::stringstream stream;
    stream << (i + 1);
    TextView* textView = new TextView(windowRoot,
                                     "../fonts/Roboto-Regular.ttf",
                                     stream.str(),
                                     20,
                                     TextView::TextAlignment::Center);
    
    double r = uniform(generator);
    double g = uniform(generator);
    double b = uniform(generator);
    textView->SetRGB(r, g, b);
    Constraint* left = new Constraint(textView,
                                      BoxAttribute::Left,
                                      Relation::EqualTo,
                                      scrollView,
                                      BoxAttribute::Left, 1.f, 2.f);
   Constraint* right = new Constraint(textView,
                                      BoxAttribute::Right,
                                      Relation::EqualTo,
                                      scrollView,
                                      BoxAttribute::Right, 1.f, -2.f);
   Constraint* top = nullptr;  
   if (i == 0) {
     top = new Constraint(textView,
               BoxAttribute::Top,
               Relation::EqualTo,
               scrollView,
               BoxAttribute::Top, 1.f, 0.f);
   } else {
     top = new Constraint(textView,
               BoxAttribute::Top,
               Relation::EqualTo,
               prevTextView,
               BoxAttribute::Bottom, 1.f, 5.f);
   }
   prevTextView = textView; // Set for next iteration.
   scrollView->AddView(textView);
   windowRoot->AddConstraint(left);
   windowRoot->AddConstraint(right);
   windowRoot->AddConstraint(top);
  }
  
  int frameidx = 0;
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

  // TODO: Eventually handle cleanup of Vulkan resources.
  delete windowRoot;
  glfwDestroyWindow(GlfwResources.window);
  glfwTerminate(); 
  
  return 0;
}
