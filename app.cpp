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

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "View.h"
#include "ScrollableView.h"
#include "ImageView.h"
#include "TextView.h"
#include "EditTextView.h"
#include "GuidelineView.h"

// TODO: memory leaks, need to move to using smart-pointers
//       eventually.

// Types
class GLFWResources {
  public:
  GLFWwindow* window;
};

static GLFWResources GlfwResources;
static constexpr int WindowWidth = 1000; // Window width in pixels.
static constexpr int WindowHeight = 500; // Window height in pixels.

void CreateGLFWResources(const char* windowName) {
  if (glfwInit() == GLFW_FALSE) {
    throw std::runtime_error("Failed to init glfw");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No context creation
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Request Window resizable
  
  GlfwResources.window = glfwCreateWindow(WindowWidth, WindowHeight, windowName, nullptr, nullptr);
  if (GlfwResources.window == nullptr) {
    throw std::runtime_error("Failed to create glfw window");
  }
}

int currentWidth = WindowWidth; 
int currentHeight = WindowHeight; 

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
  currentWidth = width;
  currentHeight = height;
}

static bool ScrollEvent = false;
static int ScrollYOffset = 0;
static int ScrollXOffset = 0;
static constexpr int SCROLL_FACTOR = 50;

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  // One scroll-click processed at a time.
  // yoffset is always either +1 or -1
  ScrollXOffset = static_cast<int>(xoffset); 
  ScrollYOffset = static_cast<int>(yoffset);
  ScrollEvent = true; 
}

static bool MouseButtonHeld = false;
static bool MouseHeldEvent = false;

static int MouseXPos = 0;
static int MouseYPos = 0; 
void mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
  // xpos and ypos is the window position of the mouse. 
  MouseXPos = xpos;
  MouseYPos = ypos;
  if (MouseButtonHeld) {
    MouseHeldEvent = true;
  }
}

static bool MouseClickEvent = false;
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    MouseClickEvent = true; 
    MouseButtonHeld = true;
  } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    MouseButtonHeld = false;
  }
}

static bool TextEvent = false;
static unsigned int characterPressed = 0;
void characterCallback(GLFWwindow* window, unsigned int codepoint) {
  characterPressed = codepoint;
  TextEvent = true;
}

static bool KeyEvent = false;
Key pressedKey;
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    pressedKey = Key::Backspace; 
    KeyEvent = true;
  } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    pressedKey = Key::RightArrow;
    KeyEvent = true;
  } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    pressedKey = Key::LeftArrow;
    KeyEvent = true;
  } else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    pressedKey = Key::Enter;
    KeyEvent = true;
  }
}

std::vector<std::string> GetFilenames() {
  std::vector<std::string> mFiles;
  for (const auto& entry : std::filesystem::directory_iterator(".")) {
    if (entry.is_regular_file()) {
      mFiles.push_back(entry.path().filename());
    }
  }
  return mFiles;
}

int main(int argc, char **argv) {
  /* Init GLFW + Create Window */
  CreateGLFWResources("Text Editor Sample App");
  glfwSetFramebufferSizeCallback(GlfwResources.window, framebufferResizeCallback);

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  
  // XXX: Don't like that I have to do this, but whatever.
  int Width2, Height2;
  glfwGetFramebufferSize(GlfwResources.window, &Width2, &Height2);
  currentWidth = Width2, currentHeight = Height2;

  WindowRoot* const windowRoot = new WindowRoot(Width2, Height2);
  if (!windowRoot || !windowRoot->Init(glfwExtensions, glfwExtensionCount)) {
    return 1; // E_INIT
  }
  glfwSetWindowUserPointer(GlfwResources.window, windowRoot);
  
  // Set Mouse position Callback
  glfwSetCursorPosCallback(GlfwResources.window, mousePositionCallback);

  // Set Mouse Button Press
  glfwSetMouseButtonCallback(GlfwResources.window, mouseButtonCallback);

  // Set Scroll Callback
  GLFWscrollfun scroll_cb = scrollCallback;
  glfwSetScrollCallback(GlfwResources.window, scroll_cb);
  
  // Set Text Keypress Callback
  glfwSetCharCallback(GlfwResources.window, characterCallback); 

  // Set Keypress Callback
  glfwSetKeyCallback(GlfwResources.window, keyCallback);
  
  /* Create Vulkan Surface using GLFW */
  VkSurfaceKHR surface; 
  VkInstance i = windowRoot->GetVkInstance();
  glfwCreateWindowSurface(i, GlfwResources.window, nullptr, &surface);
  windowRoot->SetSurface(surface);

  std::vector<std::string> filenames = GetFilenames();

  GuidelineView* guideline = new GuidelineView(windowRoot, 250.f, 75);
  windowRoot->AddView(guideline);

  TextView* filesView = new TextView(windowRoot, "fonts/Monospace.ttf", "Files", 
                                        17, TextView::TextAlignment::Center);

  filesView->SetTextRGB(1.f, 1.f, 1.f);
  Constraint* filesTop = new Constraint(filesView, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 0.f);
  Constraint* filesHeight = new Constraint(filesView, BoxAttribute::Height, 
                                              Relation::EqualTo, nullptr, 
                                              BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* filesWidth = new Constraint(filesView, BoxAttribute::Width, 
                                            Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 1.f, 250.f/*250.f*/);
  Constraint* filesLeft = new Constraint(filesView, BoxAttribute::Left, 
                                            Relation::EqualTo,
                                            windowRoot, BoxAttribute::Left, 1.f, 0.f);
  windowRoot->AddView(filesView);
  windowRoot->AddConstraint(filesTop);
  windowRoot->AddConstraint(filesHeight);
  windowRoot->AddConstraint(filesWidth);
  windowRoot->AddConstraint(filesLeft);


  ScrollableView* scrollView = new ScrollableView(windowRoot);

  Constraint* scrollLeft = new Constraint(scrollView, 
                                   BoxAttribute::Left,
                                   Relation::EqualTo,
                                   windowRoot,
                                   BoxAttribute::Left, 1.f, 0.f);
  Constraint* scrollTop = new Constraint(scrollView, 
                                            BoxAttribute::Top,
                                            Relation::EqualTo,
                                            filesView,
                                            BoxAttribute::Bottom,
                                            1.f, 0.f);
  Constraint* scrollBottom = new Constraint(scrollView, 
                                            BoxAttribute::Bottom,
                                            Relation::EqualTo,
                                            windowRoot,
                                            BoxAttribute::Bottom,
                                            1.f, -400.f);
  
  // XXX: This is where we need a CenterX Attribute 
  //      to fix the scrollview to the center of guideline!
  Constraint* scrollRight = new Constraint(scrollView, 
                                   BoxAttribute::Right,
                                   Relation::EqualTo,
                                   guideline,
                                   BoxAttribute::Left, 1.f, 0.f);
  windowRoot->AddView(scrollView);
  windowRoot->AddConstraint(scrollLeft);
  windowRoot->AddConstraint(scrollTop);
  windowRoot->AddConstraint(scrollBottom);
  windowRoot->AddConstraint(scrollRight);

  
  View* lastView = nullptr;
  int index = 0;
  EditTextView* editTextView;
  TextView* filenameView;
  for (auto& filename : filenames) {
    TextView* textBox = new TextView(windowRoot, 
                                     "fonts/Monospace.ttf", 
                                     filename, 17,
                                     TextView::TextAlignment::Left);
    // XXX: We have to capture both pointers by reference
    //      else they'll be null.
    textBox->RegisterClickEvent([filename, &editTextView, &filenameView](View* selfView) {
        std::cout << filename << " Click Event called" << std::endl; 
        std::ifstream instream(filename);
        std::stringstream buffer;
        buffer << instream.rdbuf();
        editTextView->SetText(buffer.str());
        filenameView->SetText(filename);
    });
    textBox->SetRGB(0.05, 0.05, 0.09);
    textBox->SetTextRGB(1.f, 1.f, 1.f);

    Constraint* left = new Constraint(textBox, BoxAttribute::Left, Relation::EqualTo,
                                      scrollView, BoxAttribute::Left, 1.f, 0.f);
    Constraint* right = new Constraint(textBox, BoxAttribute::Right, Relation::EqualTo,
                                      scrollView, BoxAttribute::Right, 1.0f, 0.f);
 
    Constraint* top;
    if (lastView) {
      top = new Constraint(textBox, BoxAttribute::Top, Relation::EqualTo,
                                      lastView, BoxAttribute::Bottom, 1.f, 0.f);
    } else {
      top = new Constraint(textBox, BoxAttribute::Top, Relation::EqualTo,
                           scrollView, BoxAttribute::Top, 1.f, 0.f);
    }
 
    windowRoot->AddConstraint(left);
    windowRoot->AddConstraint(right);
    windowRoot->AddConstraint(top);
    scrollView->AddView(textBox);
    lastView = textBox;
    index++;
    if (index > 20) break;
  }
  
  int currentFontSize = 16;
  EditTextView* editText = new EditTextView(windowRoot, 
                                            "fonts/Roboto-Regular.ttf", currentFontSize);

  editTextView = editText;

  editText->SetRGB(0.9f, 0.9f, 0.9f);
  editText->SetTextRGB(0.f, 0.f, 0.f);
  editText->SetCursorRGB(1.f, 0.f, 0.f);

  Constraint* rightEdit = new Constraint(editText, BoxAttribute::Right, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Right, 1.f, 0.f);
  Constraint* leftEdit = new Constraint(editText, BoxAttribute::Left, Relation::EqualTo,
                                     scrollView, BoxAttribute::Right, 1.f, 0.f);
  Constraint* topEdit = new Constraint(editText, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 100.f);
  Constraint* bottomEdit = new Constraint(editText, BoxAttribute::Bottom, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Bottom, 1.f, 0.f);

  windowRoot->AddView(editText);
  windowRoot->AddConstraint(rightEdit);
  windowRoot->AddConstraint(leftEdit);
  windowRoot->AddConstraint(topEdit);
  windowRoot->AddConstraint(bottomEdit);

  // "+" and "-" for changing font size.
  ImageView* plusView = new ImageView(windowRoot, "images/PlusWhite.png");
  plusView->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* plusTop = new Constraint(plusView, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 0.f);
  Constraint* plusHeight = new Constraint(plusView, BoxAttribute::Height, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 0.f, 100.f);
  Constraint* plusLeft = new Constraint(plusView, BoxAttribute::Left, Relation::EqualTo,
                                     filesView/*scrollView*/, BoxAttribute::Right, 1.f, 0.f);
  Constraint* plusWidth = new Constraint(plusView, BoxAttribute::Width, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 0.f, 100.f);

  windowRoot->AddView(plusView);
  windowRoot->AddConstraint(plusTop);
  windowRoot->AddConstraint(plusHeight);
  windowRoot->AddConstraint(plusWidth);
  windowRoot->AddConstraint(plusLeft);

  plusView->RegisterClickEvent([&](View* view) {
      currentFontSize = std::min<int>(50, currentFontSize+1); 
      editTextView->SetTextSize(currentFontSize);
  });
  
  // Need to maintain aspect ratio but move to center.
  // Should I put this logic in ImageView?
  ImageView* minusView = new ImageView(windowRoot, "images/MinusWhite.png");
  minusView->SetOutlineRGB(1.f, 1.f, 1.f);
  Constraint* minusTop = new Constraint(minusView, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 0.f);
  Constraint* minusHeight = new Constraint(minusView, BoxAttribute::Height, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 0.f, 100.f);
  Constraint* minusLeft = new Constraint(minusView, BoxAttribute::Left, Relation::EqualTo,
                                     plusView, BoxAttribute::Right, 1.f, 0.f);
  Constraint* minusWidth = new Constraint(minusView, BoxAttribute::Width, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 0.f, 100.f);
  minusView->RegisterClickEvent([&](View* view) {
      currentFontSize = std::max<int>(6, currentFontSize-1); 
      editTextView->SetTextSize(currentFontSize);
  });

  windowRoot->AddView(minusView);
  windowRoot->AddConstraint(minusTop);
  windowRoot->AddConstraint(minusHeight);
  windowRoot->AddConstraint(minusWidth);
  windowRoot->AddConstraint(minusLeft);

  // Save Button too
  TextView* saveButton = new TextView(windowRoot, "fonts/Monospace.ttf", 
                                      "Save", 17);
  saveButton->SetRGB(0.05, 0.05, 0.09);
  saveButton->SetTextRGB(1.f, 1.f, 1.f);
  Constraint* saveTop = new Constraint(saveButton, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 0.f);
  Constraint* saveHeight = new Constraint(saveButton, BoxAttribute::Height, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* saveRight = new Constraint(saveButton, BoxAttribute::Right, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Right, 1.f, 0.f);
  
  // XXX: This is needed for some reason, look into it. 
  //      Ideally the generated constraint from the text content
  //      would've worked....
  Constraint* saveWidth = new Constraint(saveButton, BoxAttribute::Width, Relation::EqualTo,
                                     nullptr, BoxAttribute::NoAttribute, 1.f, 200.f);

  // TODO: Add Click Event for Text to Save.
  
  windowRoot->AddView(saveButton);
  windowRoot->AddConstraint(saveTop);
  windowRoot->AddConstraint(saveHeight);
  windowRoot->AddConstraint(saveWidth);
  windowRoot->AddConstraint(saveRight);

  // TextView for File Name
  filenameView = new TextView(windowRoot, "fonts/Monospace.ttf", "", 
                                        17, TextView::TextAlignment::Center);

  filenameView->SetTextRGB(1.f, 1.f, 1.f);
  Constraint* filenameTop = new Constraint(filenameView, BoxAttribute::Top, Relation::EqualTo,
                                     windowRoot, BoxAttribute::Top, 1.f, 0.f);
  Constraint* filenameHeight = new Constraint(filenameView, BoxAttribute::Height, 
                                              Relation::EqualTo, nullptr, 
                                              BoxAttribute::NoAttribute, 1.f, 100.f);
  Constraint* filenameRight = new Constraint(filenameView, BoxAttribute::Right, 
                                            Relation::EqualTo,
                                            saveButton, BoxAttribute::Left, 1.f, 0.f);
  Constraint* filenameLeft = new Constraint(filenameView , BoxAttribute::Left, 
                                            Relation::EqualTo,
                                            minusView, BoxAttribute::Right, 1.f, 0.f);
  windowRoot->AddView(filenameView);
  windowRoot->AddConstraint(filenameTop);
  windowRoot->AddConstraint(filenameHeight);
  windowRoot->AddConstraint(filenameRight);
  windowRoot->AddConstraint(filenameLeft);


  // Add Another Edit Text Box and Open Button for opening any file!
  // Rather than just the ones in scrollview.

  int frameidx = 0;
  while (!glfwWindowShouldClose(GlfwResources.window)) {
    std::cout << "Frame Index: " << frameidx++ << std::endl;
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
