#include <array>
#include <chrono>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "Setup.h"

#include "../TextView.h"

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("TextView Example");
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
  VkInstance instance = windowRoot->GetVkInstance();
  glfwCreateWindowSurface(instance, GlfwResources.window, nullptr, &surface);
  windowRoot->SetSurface(surface);
  
  TextView* textView = new TextView(windowRoot, "../fonts/Monospace.ttf", "Centered Text",
                                    20, TextView::TextAlignment::Center);
  textView->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topConstraint = new Constraint(textView, BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* leftConstraint = new Constraint(textView, BoxAttribute::Left, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* rightConstraint = new Constraint(textView, BoxAttribute::Right, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Right, 1.f, -100.f);


  bool toggle = false;
  textView->RegisterClickEvent([&toggle](View* view) {
      TextView* textView = dynamic_cast<TextView*>(view);
      if (textView) {
        toggle ^= true;
        if (toggle) {
          textView->SetTextRGB(0.f, 0.f, 0.f);
          textView->SetRGB(1.f, 1.f, 1.f);
        } else {
          textView->SetTextRGB(1.f, 1.f, 1.f);
          textView->SetRGB(0.f, 0.f, 0.f);
        }
      }
  });
 
 
  windowRoot->AddView(textView);
  windowRoot->AddConstraint(topConstraint);
  windowRoot->AddConstraint(leftConstraint);
  windowRoot->AddConstraint(rightConstraint);

  TextView* textViewLeftAligned = new TextView(windowRoot, "../fonts/Roboto-Regular.ttf", 
                                    "Left-Aligned",
                                    30, TextView::TextAlignment::Left);
  textViewLeftAligned->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topConstraintLeftAligned = new Constraint(textViewLeftAligned, 
                                    BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 300.f);
  Constraint* leftConstraintLeftAligned = new Constraint(textViewLeftAligned, 
                                    BoxAttribute::Left, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Left, 1.f, 50.f);
  Constraint* rightConstraintLeftAligned = new Constraint(textViewLeftAligned, 
                                    BoxAttribute::Right, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Right, 1.f, -50.f);
 
 
  windowRoot->AddView(textViewLeftAligned);
  windowRoot->AddConstraint(topConstraintLeftAligned);
  windowRoot->AddConstraint(leftConstraintLeftAligned);
  windowRoot->AddConstraint(rightConstraintLeftAligned);

  TextView* textViewRightAligned = new TextView(windowRoot, "../fonts/Roboto-Regular.ttf", 
                                    "Right-Aligned",
                                    30, TextView::TextAlignment::Right);
  textViewRightAligned->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topConstraintRightAligned = new Constraint(textViewRightAligned,
                                    BoxAttribute::Top, 
                                    Relation::EqualTo, textViewLeftAligned,
                                    BoxAttribute::Bottom, 1.f, 50.f);
  Constraint* leftConstraintRightAligned = new Constraint(textViewRightAligned, 
                                    BoxAttribute::Left, 
                                    Relation::EqualTo, textViewLeftAligned,
                                    BoxAttribute::Left, 1.f, 0.f);
  Constraint* rightConstraintRightAligned = new Constraint(textViewRightAligned, 
                                    BoxAttribute::Right, 
                                    Relation::EqualTo, textViewLeftAligned,
                                    BoxAttribute::Right, 1.f, 0.f);
 
  windowRoot->AddView(textViewRightAligned);
  windowRoot->AddConstraint(topConstraintRightAligned);
  windowRoot->AddConstraint(leftConstraintRightAligned);
  windowRoot->AddConstraint(rightConstraintRightAligned);

  TextView* textViewContent = new TextView(windowRoot, "../fonts/Monospace.ttf", 
                                           "Content-Wrapped TextView",
                                           30, TextView::TextAlignment::Center);
  Constraint* bottomConstraintContent = new Constraint(textViewContent,
                                                BoxAttribute::Bottom,
                                                Relation::EqualTo, 
                                                windowRoot,
                                                BoxAttribute::Bottom, 1.f, -50);
  Constraint* rightConstraintContent = new Constraint(textViewContent,
                                                BoxAttribute::Right,
                                                Relation::EqualTo, 
                                                windowRoot,
                                                BoxAttribute::Right, 1.f, -50);

  textViewContent->SetTextRGB(0.75f, 0.2f, 0.2f);
  textViewContent->SetOutlineRGB(1.f, 1.f, 1.f);
  windowRoot->AddView(textViewContent);
  windowRoot->AddConstraint(bottomConstraintContent);
  windowRoot->AddConstraint(rightConstraintContent);

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
