#include <array>
#include <chrono>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "Setup.h"

#include "../ImageView.h"

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("ImageView Example");
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
  
  ImageView* imageView = new ImageView(windowRoot, "../images/google.png");
  Constraint* topConstraint = new Constraint(imageView, BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* leftConstraint = new Constraint(imageView, BoxAttribute::Left, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
 
  windowRoot->AddView(imageView);
  windowRoot->AddConstraint(topConstraint);
  windowRoot->AddConstraint(leftConstraint);

  ImageView* imageViewScrollable = new ImageView(windowRoot, "../images/apple.png", true);
  imageViewScrollable->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topConstraintScrollable = new Constraint(imageViewScrollable, 
                                    BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 500.f);

  Constraint* heightConstraintScrollable = new Constraint(imageViewScrollable, 
                                    BoxAttribute::Height, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 0.f, 110.f);

  Constraint* leftConstraintScrollable = new Constraint(imageViewScrollable, 
                                    BoxAttribute::Left, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);
 
  windowRoot->AddView(imageViewScrollable);
  windowRoot->AddConstraint(topConstraintScrollable);
  windowRoot->AddConstraint(heightConstraintScrollable);
  windowRoot->AddConstraint(leftConstraintScrollable);
  
  ImageView* scaledImageView = new ImageView(windowRoot, "../images/RedArrow.png");
  scaledImageView->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* topConstraintScaled = new Constraint(scaledImageView, 
                                    BoxAttribute::Top, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 1.f, 100.f);

  Constraint* heightConstraintScaled = new Constraint(scaledImageView, 
                                    BoxAttribute::Height, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 0.f, 100.f);
  Constraint* widthConstraintScaled = new Constraint(scaledImageView, 
                                    BoxAttribute::Width, 
                                    Relation::EqualTo, nullptr,
                                    BoxAttribute::NoAttribute, 0.f, 300.f);

  Constraint* rightConstraintScaled = new Constraint(scaledImageView, 
                                    BoxAttribute::Right, 
                                    Relation::EqualTo, windowRoot,
                                    BoxAttribute::Right, 1.f, 0.f);
 
  windowRoot->AddView(scaledImageView);
  windowRoot->AddConstraint(topConstraintScaled);
  windowRoot->AddConstraint(heightConstraintScaled);
  windowRoot->AddConstraint(widthConstraintScaled);
  windowRoot->AddConstraint(rightConstraintScaled);

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
