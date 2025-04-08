#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <unordered_set>

#include "Font.h"
#include "Image.h"

// TODO: Better Resource Management.
//       Loading and Unloading of Textures at the necessary moments, etc.
//        
struct __attribute__((packed)) vec2 {
  float x;
  float y;
};

struct Rect {
  int offsetX;
  int offsetY;
  int sizeX;
  int sizeY;
};

class Graphics2D {
public:
  // Construction / Destruction
  Graphics2D() = default;
  
  // Destructor
  ~Graphics2D();

  Graphics2D(const Graphics2D &) = delete;

  // Operators
  Graphics2D& operator=(const Graphics2D&) = delete;
  
  // Create VkInstance. Enables given Instance Extensions
  // as well as Extensions and Layers expressed 
  // in "enabledInstanceExtensions" and "enabledInstanceLayers"
  bool Init(const char **enabledExtensions, const uint32_t extensionCount);
 
  // VkSurface is set. Then majority of 
  // Vulkan Resources are created.
  // eg: Logical Device, Swapchain,
  //     CommandPool/Buffer, RenderPass,
  //     etc.
  void SetSurface(VkSurfaceKHR surface);
  
  // Resize Swapchain. Necessary whenever underlying
  // Surface changes.
  bool Resize();
  
  // Blocks until Command Buffer has finished executing.
  // Then Acquires Next Swapchain Image.
  // Starts to Record Command Buffer
  // and starts Render Pass
  void BeginRecording();
  
  // Ends Render Pass and Command Buffer
  void EndRecording();
  
  // Submits Command Buffer for Execution
  // and Current Swapchain Image for 
  // Presentation
  void Present();
 
  // Set Color which will be used in subsequent Draw Calls
  void SetColor(float r, float g, float b, float a);

  // Set Line Width, Width of Rasterized Line Segments.
  // Used in Graphics2D::DrawOutline
  void SetLineWidth(float lineWidth); 

  // Revert to Default Line Width
  void UnsetLineWidth();
  
  // Set Scissor. Returns currently-set Scissor, if one is.
  void SetScissor(const Rect& scissor, Rect& originalScissor);
  void SetScissor(int offsetX, int offsetY, 
                  int sizeX, int sizeY, Rect& originalScissor);
  
  // Apply no Scissor. Render to entire Framebuffer
  void UnsetScissor();

  // Draws a Flat-colored Box at given coordinates.
  // Uses the Set Scissor, if one is set.
  void DrawRect(int32_t l, int32_t t, int32_t r, int32_t b);
  
  // Draws outline of a Box at given coordinates.
  // Uses Set Scissor and Set Line Width, if one is set.
  void DrawOutline(int32_t l, int32_t t, int32_t r, int32_t b);
  
  // Draws Box at given coordintates with ImageId_t acting 
  // as a texture. Texture will be sampled to fit box.
  // Uses Set Scissior, if one is set.
  void DrawTexturedRect(ImageId_t imgId, int32_t l, int32_t t, 
                        int32_t r, int32_t b);

  // Draws Box at given coordintates with ImageId_t acting 
  // as texture. 4 Texture coords are passed through to 
  // shader for Top-Left, Top-Right, Bottom-Right,
  // Bottom-Left vertices, respectively.
  // Uses Set Scissor, if one is set.
  void DrawTexturedRect(ImageId_t imageId, int32_t l, int32_t t, 
                        int32_t r, int32_t b, 
                        const std::vector<vec2>& textureCoords);
  
  // Draws string on baseline specified by baselineX, baselineY.
  // Uses FontId_t to specify font and point size.
  // Uses Set Scissor, if one is set.
  void DrawText(FontId_t fontId, const std::string& str, 
                int32_t baselineX, int32_t baselineY);
  
  // Creates Image Resource and loads it onto GPU.
  // Returns ImageId_t for user to reference 
  // images in Draw calls and GetImageInfo.
  ImageId_t AddImage(const std::string& imageFile);
  
  // Get ImageInfo given ImageId_t 
  ImageInfo GetImageInfo(ImageId_t id) const;

  // Creates Font Atlas/Image and loads it onto GPU
  // using given a fontFile and pointSize.
  // Returns FontId_t for user to refer to 
  // a specific Font/Size when calling 
  // DrawText
  FontId_t AddFont(const std::string& fontFile, const uint32_t pointSize);
  
  // Get FontInfo given FontId_t 
  const FontInfo* GetFontInfo(FontId_t fontId);
  
  VkInstance GetVkInstance() const { return mVulkanInstance; }

 private:
  // Switch to using these handles!

  VkInstance mVulkanInstance;
  static constexpr uint32_t kFRAMES_IN_FLIGHT = 2;

  struct {
    VkSwapchainKHR swapchain {VK_NULL_HANDLE};
    VkExtent2D extent;
    VkFormat format;
    std::vector<VkImage> images;
    uint32_t mCurrentSwapchainIdx;  // Index of Swapchain Image which was 
                                    // most recently acquired when calling
                                    // Graphics2D::BeginRecording.
 
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    bool Resize();
  } mSwapchain;

  struct {
    VkSurfaceKHR surface{VK_NULL_HANDLE}; // Platform
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR surfaceFormat;
  } mSurface;

  struct {
    // Physical Device,
    // Physical Device Index, 
    // mQueueFamilyIdx
    // Logical Device
    // Queue, PresentQueue 
    
    VkPhysicalDevice physicalDevice;
    uint32_t mPhysicalDeviceIdx;      // Index into array of VkPhysicalDevices
                                      // returned by vkEnumeratePhysicalDevices
                                      // indicating which Physical Device we've selected.
    VkDevice logicalDevice{VK_NULL_HANDLE};
    VkQueue queue{VK_NULL_HANDLE};
    uint32_t mQueueFamilyIdx;       // Index into array of VkQueueFamilyProperties
                                    // returned by vkGetPhysicalDeviceQueueFamilyProperties.
                                    // indicated which Queue Family we've selected
                                    // to use.
    VkQueue presentQueue{VK_NULL_HANDLE};
  } mDevice;

  struct Pipeline {
    VkDescriptorSetLayout descriptorLayouts{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
  };
  std::array<Pipeline, 4> mPipelines;
  static constexpr size_t LinePipelineIdx = 0;
  static constexpr size_t FlatPipelineIdx = 1;
  static constexpr size_t TexturedPipelineIdx = 2;
  static constexpr size_t TextPipelineIdx = 3;

  struct {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers{VK_NULL_HANDLE};
    int commandBufferIndex = 0; // Current Command Buffer is use during 
                                // Frame.
  } mCommand;

  struct {
    VkRenderPass renderPass{VK_NULL_HANDLE};
  } mRendering;
  
  struct {
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
  } mDescriptors;
  
  struct {
    std::vector<VkSemaphore> renderCompleteSemaphores;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkFence> commandBufferFences;
  } mSync;
  
  // Currently used for Transferring Resources to GPU 
  struct TemporaryCommandBuffer {
    TemporaryCommandBuffer(VkDevice dev, VkCommandPool pool) :
    device(dev), commandPool(pool), commandBuffer(VK_NULL_HANDLE) {
      VkCommandBufferAllocateInfo cbInfo{};
      cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cbInfo.pNext = nullptr;
      cbInfo.commandPool = commandPool;
      cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cbInfo.commandBufferCount = 1;
      vkAllocateCommandBuffers(device, &cbInfo, &commandBuffer);
    }

    ~TemporaryCommandBuffer() {
      vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    VkDevice device;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
  };

  void UpdateDescriptorSet(VkDescriptorSet descriptorSet, VkSampler sampler,
                           VkImageView imageView, VkImageLayout imageLayout);
  
  // Image Retrieval
  const Image* GetImage(ImageId_t id) const;
  std::array<VkDescriptorSet, 2> GetImageDescriptors(ImageId_t id) const;
  
  // Font Retrieval
  std::array<VkDescriptorSet, 2> GetFontDescriptors(FontId_t id) const;
  
  //
  // Vulkan Resource Creation
  // 

  void ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats, VkSurfaceFormatKHR &formatOut) const;

  // Creates Vulkan Resources below.
  // Called After VkSurface is set.
  bool CreateResources();
  
  // Create VkSwapchain 
  bool CreateSwapchain();
  // Create VkCommandBuffer
  bool CreateCommandBuffer();

  // Create VkRenderPass
  bool CreateRenderPass();

  // Create VkFramebuffer
  bool CreateFramebuffers();
  
  // Create VkShaderModule using given file.
  VkShaderModule CreateShaderModule(VkDevice device, const char* shaderFile); 
  VkShaderModule CreateShaderModule(VkDevice device, const uint8_t* shaderCode,
                                    const int byteSize); 
  
  // Create Graphics Pipeline (VkPipeline) for 
  // drawing Textured Quads
  bool CreateTexturedPipeline();

  // Create Graphics Pipeline (VkPipeline) for 
  // drawing Outline of a Quad
  bool CreateLinePipeline();
  
  // Create Graphics Pipeline (VkPipeline) for 
  // drawing flat-colored Quad
  bool CreateFlatColoredPipeline();
  
  // Create Graphics Pipeline (VkPipeline) for 
  // drawing Text
  bool CreateTextPipeline();
  
  // Create Descriptor Pool (VkDescriptorPool)
  // for allocating Descriptor Sets.
  void CreateDescriptorPool();
  
  // Create Descriptor Sets for Text Atlas
  // and Texture.
  // Stored in mFontDescriptors and mImageDescriptors respectively.
  std::array<VkDescriptorSet, 2> CreateFontDescriptorSets();
  std::array<VkDescriptorSet, 2> CreateTexturedDescriptorSets();
  
  // Create the Following synchorization
  // primitives:
  //
  // Used to synchronize rendering, presentation,
  // and command buffer completion.
  bool CreateSyncPrimitives();

  void DestroySwapchain();
 
  uint32_t GetQueueFamilyIdx() const {
    return mDevice.mQueueFamilyIdx;
  }

  void SetQueueFamilyIdx(uint32_t idx) {
    mDevice.mQueueFamilyIdx = idx;
  }

  uint32_t GetPhysicalDeviceIdx() const {
    return mDevice.mPhysicalDeviceIdx;
  }

  void SetPhysicalDeviceIdx(uint32_t idx) {
    mDevice.mPhysicalDeviceIdx = idx;
  }

  void SetCurrentSwapchainIdx(uint32_t idx) {
    mSwapchain.mCurrentSwapchainIdx = idx;
  }

  uint32_t GetCurrentSwapchainIdx() const {
    return mSwapchain.mCurrentSwapchainIdx;
  }

  uint32_t GetCommandBufferIdx() const {
    return mCommand.commandBufferIndex;
  }

  void SetCurrentCommandBufferIdx(uint32_t idx) {
    mCommand.commandBufferIndex = idx;
  }

  std::array<float, 4> mRGB; // Could be a function like PaintBrush
                            // along with VkScissor, etc.?
                            // Could have, LineColor, and FillColor too.
                            // Line Width...
  VkViewport mDefaultViewport;
  VkRect2D mDefaultScissor;
  VkRect2D mSetScissor;
  bool mScissorIsSet = false;
  bool mLineWidthIsSet = false;

  float mDefaultLineWidth = 2.0f;
  float mCurrentLineWidth = 1.0f;

  Text mText;

  std::unordered_map<std::string, ImageId_t> mImageMap;
  std::unordered_map<ImageId_t, Image> mImageResources;
  std::unordered_map<ImageId_t, std::array<VkDescriptorSet, 2>> mImageDescriptors;
  std::unordered_map<FontId_t, std::array<VkDescriptorSet, 2>> mFontDescriptors;
  
  // Used to ensure we only update a Descriptor Set once every frame.
  std::unordered_set<FontId_t> mFontDescriptorsUpdated;
  std::unordered_set<ImageId_t> mImageDescriptorsUpdated;
};
