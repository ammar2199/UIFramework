#pragma once

#include "../View.h"

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

static int currentWidth = WindowWidth; 
static int currentHeight = WindowHeight; 
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
  }
}

void SetupEvents(GLFWwindow* window) {
 glfwSetFramebufferSizeCallback(GlfwResources.window, framebufferResizeCallback);

  // XXX: Don't like that I have to do this, but whatever.
  int Width2, Height2;
  glfwGetFramebufferSize(window, &Width2, &Height2);
  currentWidth = Width2, currentHeight = Height2;

  //glfwSetWindowUserPointer(window, windowRoot);
  
  // Set Mouse position Callback
  glfwSetCursorPosCallback(window, mousePositionCallback);

  // Set Mouse Button Press
  glfwSetMouseButtonCallback(window, mouseButtonCallback);

  // Set Scroll Callback
  GLFWscrollfun scroll_cb = scrollCallback;
  glfwSetScrollCallback(window, scroll_cb);
  
  // Set Text Keypress Callback
  glfwSetCharCallback(window, characterCallback); 

  // Set Keypress Callback
  glfwSetKeyCallback(window, keyCallback);
 
}
