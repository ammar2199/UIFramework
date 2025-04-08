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

#include "../GuidelineView.h"


class VisibleGuidelineView : public GuidelineView {
 public:
   VisibleGuidelineView(WindowRoot* window, double constant, 
                        double size,
                        GuidelineView::Orientation orientation) : 
                        GuidelineView(window, constant, size, orientation) {}

  virtual void draw() {
    GetGraphics()->SetColor(1.f, 1.f, 1.f, 1.f);
    GetGraphics()->DrawRect(GetLeft(), GetTop(), GetRight(), GetBottom());
  }
};

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("GuidelineView Example");
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

  VisibleGuidelineView* guidelineVertical = new VisibleGuidelineView(windowRoot, 250.f, 50, GuidelineView::Orientation::Vertical);
  windowRoot->AddView(guidelineVertical);
 
  VisibleGuidelineView* guidelineHorizontal = new VisibleGuidelineView(windowRoot, 250.f, 20, GuidelineView::Orientation::Horizontal);
  windowRoot->AddView(guidelineHorizontal);

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
