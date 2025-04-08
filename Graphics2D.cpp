#include "Graphics2D.h"

#include "Image.h"
#include "Font.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <vector>
#include <unordered_set>

#include <sys/stat.h>

#include "vulkan/vulkan.h"

// Shaders
#include "shaderFlatVS.h"
#include "shaderFlatFS.h"
#include "shaderLineVS.h"
#include "shaderLineFS.h"
#include "shaderTexturedVS.h"
#include "shaderTexturedFS.h"
#include "shaderTextFS.h"

#define VULKAN_CALL_CHECK(res)                                                 \
  if (res != VK_SUCCESS) {                                                     \
    fprintf(stderr, "Failed Vulkan Call: %s: %d\n", __PRETTY_FUNCTION__, res); \
    return false;                                                              \
  }

static const std::vector<const char *> enabledInstanceLayers = {
    "VK_LAYER_KHRONOS_validation"};
static const std::vector<const char *> enabledInstanceExtensions = {
    "VK_KHR_surface", "VK_KHR_xlib_surface"};
const std::vector<const char *> enabledDeviceExtensions = {"VK_KHR_swapchain"};


// Used to Populate Push Constant
// Packed to match std430 layout
struct __attribute__((packed)) PushConstantQuadInfo {
  int32_t QuadCoords[4]; // In FrameBuffer Coordinates.
  uint32_t frameBufferSize[2];
  float rgb[4];
  vec2 textureCoords[4]; // Normalized Coordinates [0, 1]
                         // Top Left, Top Right, Bottom Right, BottomLeft
};

// 
// INITIALIZATION
//

bool Graphics2D::Init(const char **enabledExtensions, const uint32_t extensionCount) {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.apiVersion =
      VK_MAKE_API_VERSION(0, 1, 3, 0);

  std::unordered_set<std::string> extensionSet(enabledInstanceExtensions.begin(), 
                                                     enabledInstanceExtensions.end());
  for (int i=0; i<extensionCount; i++) {
    extensionSet.insert(enabledExtensions[i]);
  }

  std::vector<const char*> instanceExtensions;
  std::for_each(extensionSet.begin(), extensionSet.end(), [&](const std::string& s) {
      instanceExtensions.push_back(s.c_str());
  });

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pNext = nullptr;
  ci.pApplicationInfo = &appInfo;
  ci.enabledLayerCount = enabledInstanceLayers.size();
  ci.ppEnabledLayerNames = enabledInstanceLayers.data();
  ci.enabledExtensionCount = instanceExtensions.size();
  ci.ppEnabledExtensionNames = instanceExtensions.data();
  if (vkCreateInstance(&ci, nullptr, &mVulkanInstance) != VK_SUCCESS) {
    throw std::runtime_error("Failed Vulkan Call: vkCreateInstance");
  }
  return true;
}

void Graphics2D::SetSurface(VkSurfaceKHR surface) {
  mSurface.surface = surface;
  CreateResources(); // Create Vulkan Resources
}

bool Graphics2D::Resize() {
  vkDeviceWaitIdle(mDevice.logicalDevice); // CPU waits for all operations
                                            // submitted to queues
                                            // thus far to be completed.
  DestroySwapchain();
  CreateSwapchain();
  CreateFramebuffers();
  return true;
}

Graphics2D::~Graphics2D() {
  vkDeviceWaitIdle(mDevice.logicalDevice);

  // Destroy Swapchain Resources
  DestroySwapchain();

  for (VkSemaphore sem : mSync.renderCompleteSemaphores) {
    vkDestroySemaphore(mDevice.logicalDevice, sem, nullptr);
  }

  for (VkSemaphore sem : mSync.imageAvailableSemaphores) {
    vkDestroySemaphore(mDevice.logicalDevice, sem, nullptr);
  }

  for (VkFence fence : mSync.commandBufferFences) {
    vkDestroyFence(mDevice.logicalDevice, fence, nullptr);
  }

  vkDestroyDescriptorPool(mDevice.logicalDevice, mDescriptors.descriptorPool, nullptr);
  
  vkDestroyPipeline(mDevice.logicalDevice, mPipelines[LinePipelineIdx].pipeline, nullptr);
  vkDestroyPipeline(mDevice.logicalDevice, mPipelines[FlatPipelineIdx].pipeline, nullptr);
  vkDestroyPipeline(mDevice.logicalDevice, mPipelines[TexturedPipelineIdx].pipeline, nullptr);
  vkDestroyPipeline(mDevice.logicalDevice, mPipelines[TextPipelineIdx].pipeline, nullptr);

  vkDestroyPipelineLayout(mDevice.logicalDevice, mPipelines[LinePipelineIdx].layout, nullptr);
  vkDestroyPipelineLayout(mDevice.logicalDevice, mPipelines[FlatPipelineIdx].layout, nullptr);
  vkDestroyPipelineLayout(mDevice.logicalDevice, mPipelines[TexturedPipelineIdx].layout, nullptr);
  vkDestroyPipelineLayout(mDevice.logicalDevice, mPipelines[TextPipelineIdx].layout, nullptr);

  vkDestroyDescriptorSetLayout(mDevice.logicalDevice, mPipelines[LinePipelineIdx].descriptorLayouts, nullptr);
  vkDestroyDescriptorSetLayout(mDevice.logicalDevice, mPipelines[FlatPipelineIdx].descriptorLayouts, nullptr);
  vkDestroyDescriptorSetLayout(mDevice.logicalDevice, mPipelines[TexturedPipelineIdx].descriptorLayouts, nullptr);
  vkDestroyDescriptorSetLayout(mDevice.logicalDevice, mPipelines[TextPipelineIdx].descriptorLayouts, nullptr);

  vkDestroyRenderPass(mDevice.logicalDevice, mRendering.renderPass, nullptr);
  
  vkDestroyCommandPool(mDevice.logicalDevice, mCommand.commandPool, nullptr);
  
  mImageResources.clear();

  mText.Destroy();

  vkDestroyDevice(mDevice.logicalDevice, nullptr);
  vkDestroySurfaceKHR(mVulkanInstance, mSurface.surface, nullptr);
  vkDestroyInstance(mVulkanInstance, nullptr); 
}

void Graphics2D::DestroySwapchain() {
  for (VkFramebuffer fb : mSwapchain.framebuffers) {
    vkDestroyFramebuffer(mDevice.logicalDevice, fb, nullptr);
  }
  mSwapchain.framebuffers.clear();

  for (VkImageView iv : mSwapchain.imageViews) {
    vkDestroyImageView(mDevice.logicalDevice, iv, nullptr);
  }
  mSwapchain.imageViews.clear(); 
  
  vkDestroySwapchainKHR(mDevice.logicalDevice, mSwapchain.swapchain, nullptr);
  mSwapchain.images.clear();
}
 

// RENDERING LOOP CALLS 
//
// 1. BeginRecording()
//    - Record Draw Calls
// 2. EndRecording() 
// 3. Present

void Graphics2D::BeginRecording() {
  // Block until command buffer has finished executing.
  vkWaitForFences(mDevice.logicalDevice, 1, &mSync.commandBufferFences[GetCommandBufferIdx()],
                  VK_TRUE, UINT64_MAX);
  vkResetFences(mDevice.logicalDevice, 1, &mSync.commandBufferFences[GetCommandBufferIdx()]);
  
  uint32_t swapIdx;
  VkResult result = vkAcquireNextImageKHR(mDevice.logicalDevice,
                                          mSwapchain.swapchain, UINT64_MAX,
                                          mSync.imageAvailableSemaphores[GetCommandBufferIdx()],
                                          VK_NULL_HANDLE, &swapIdx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    Resize();
    return;
  }

  if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR)) {
    throw std::runtime_error("Failed to acquire image");
  }

  SetCurrentSwapchainIdx(swapIdx);

  vkResetCommandBuffer(mCommand.commandBuffers[GetCommandBufferIdx()], 0);
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.pNext = nullptr;
  beginInfo.flags = 0;
  vkBeginCommandBuffer(mCommand.commandBuffers[GetCommandBufferIdx()], &beginInfo);

  // Start RenderPass too.
  VkRenderPassBeginInfo beginRP{};
  beginRP.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginRP.pNext = nullptr;
  beginRP.renderPass = mRendering.renderPass;
  beginRP.framebuffer = mSwapchain.framebuffers[GetCurrentSwapchainIdx()];
  beginRP.renderArea.offset = {0, 0};
  beginRP.renderArea.extent = mSwapchain.extent;

  VkClearValue clearColor = {{{0.f, 0.f, 0.f, 1.f}}};
  beginRP.clearValueCount = 1;
  beginRP.pClearValues = &clearColor;
  vkCmdBeginRenderPass(mCommand.commandBuffers[GetCommandBufferIdx()], &beginRP,
                       VK_SUBPASS_CONTENTS_INLINE);
  
  mFontDescriptorsUpdated.clear();
  mImageDescriptorsUpdated.clear();
}

void Graphics2D::EndRecording() {
  // End Render Pass and Command Buffer Recording
  vkCmdEndRenderPass(mCommand.commandBuffers[GetCommandBufferIdx()]); 
  vkEndCommandBuffer(mCommand.commandBuffers[GetCommandBufferIdx()]);
}

void Graphics2D::Present() {
  VkPipelineStageFlags waitStages =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo subInfo{};
  subInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  subInfo.pNext = nullptr;
  subInfo.waitSemaphoreCount = 1;
  subInfo.pWaitSemaphores = &mSync.imageAvailableSemaphores[GetCommandBufferIdx()];
  subInfo.pWaitDstStageMask = &waitStages;
  subInfo.commandBufferCount = 1;
  subInfo.pCommandBuffers = &mCommand.commandBuffers[GetCommandBufferIdx()];
  subInfo.signalSemaphoreCount = 1;
  subInfo.pSignalSemaphores = &mSync.renderCompleteSemaphores[GetCommandBufferIdx()];
  vkQueueSubmit(mDevice.queue, 1, &subInfo, mSync.commandBufferFences[GetCommandBufferIdx()]);

  // Present!
  const uint32_t swapIdx = GetCurrentSwapchainIdx();

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &mSync.renderCompleteSemaphores[GetCommandBufferIdx()];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &mSwapchain.swapchain;
  presentInfo.pImageIndices = &swapIdx;
  VkResult res = vkQueuePresentKHR(mDevice.queue, &presentInfo);

  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
    Resize();
  }
  else if (res != VK_SUCCESS) {
    throw std::runtime_error("Queue Present Issues");
  }
  
  uint32_t nextCommandBufferIdx = (GetCommandBufferIdx() + 1) % kFRAMES_IN_FLIGHT;
  SetCurrentSwapchainIdx(nextCommandBufferIdx);
}

// 
// DRAW CALLS
//

void Graphics2D::DrawRect(int32_t l, int32_t t, int32_t r, int32_t b) {
  // Bind the Flat-Colored Pipeline. 
  vkCmdBindPipeline(mCommand.commandBuffers[GetCommandBufferIdx()], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[FlatPipelineIdx].pipeline);
  
  // Set Viewport Transform
  vkCmdSetViewport(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultViewport); 
  
  // Set Scissor
  if (mScissorIsSet) {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mSetScissor);
  } else {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultScissor);
  }

  PushConstantQuadInfo quadInfo{
      {l, t, r, b},
      {mSwapchain.extent.width,
       mSwapchain.extent.height}, // Framebuffer Size.
      {mRGB[0], mRGB[1], mRGB[2], mRGB[3]}};
  
  vkCmdPushConstants(mCommand.commandBuffers[GetCommandBufferIdx()], 
                     mPipelines[FlatPipelineIdx].layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 
                     0, sizeof(quadInfo),
                     &quadInfo);
  vkCmdDraw(mCommand.commandBuffers[GetCommandBufferIdx()], 6, 1, 0, 0);
}

void Graphics2D::DrawOutline(int32_t l, int32_t t, int32_t r, int32_t b) {
  // Bind the Line Pipeline
  vkCmdBindPipeline(mCommand.commandBuffers[GetCommandBufferIdx()], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[LinePipelineIdx].pipeline);
  
  // Set Viewport Transform
  vkCmdSetViewport(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultViewport); 
  
  // Set Scissor
  if (mScissorIsSet) {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mSetScissor);
  } else {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultScissor);
  }

  // Set Line Width
  if (mLineWidthIsSet) {
    vkCmdSetLineWidth(mCommand.commandBuffers[GetCommandBufferIdx()], mDefaultLineWidth); 
  } else {
    vkCmdSetLineWidth(mCommand.commandBuffers[GetCommandBufferIdx()], mCurrentLineWidth); 
  }

  PushConstantQuadInfo quadInfo{
      {l, t, r, b},
      {mSwapchain.extent.width,
       mSwapchain.extent.height}, // Framebuffer Size.
      {mRGB[0], mRGB[1], mRGB[2], mRGB[3]}};
  
  vkCmdPushConstants(mCommand.commandBuffers[GetCommandBufferIdx()], 
                     mPipelines[LinePipelineIdx].layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 
                     0, sizeof(quadInfo),
                     &quadInfo);
  vkCmdDraw(mCommand.commandBuffers[GetCommandBufferIdx()], 5, 1, 0, 0);
}

void Graphics2D::DrawTexturedRect(ImageId_t imgId, int32_t l, int32_t t, 
                                  int32_t r, int32_t b) {
  const Image* image = GetImage(imgId);
  std::array<VkDescriptorSet, 2> imageDescriptors = GetImageDescriptors(imgId);
  if (!image || imageDescriptors[0] == VK_NULL_HANDLE) {
    return;
  }

  // Bind the Textured-Quad Pipeline. 
  vkCmdBindPipeline(mCommand.commandBuffers[GetCommandBufferIdx()], 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    mPipelines[TexturedPipelineIdx].pipeline);
  
  // Set Viewport Transform
  vkCmdSetViewport(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultViewport); 
  
  // Set Scissor
  if (mScissorIsSet) {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mSetScissor);
  } else {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultScissor);
  }
  

  if (mImageDescriptorsUpdated.find(imgId) == mImageDescriptorsUpdated.end()) {  
    // Update Descriptor Set with Image
    UpdateDescriptorSet(imageDescriptors[GetCommandBufferIdx()],
                        image->GetSampler(), 
                        image->GetImageView(), 
                        image->GetImageLayout());
    mImageDescriptorsUpdated.insert(imgId);
  }

  vkCmdBindDescriptorSets(mCommand.commandBuffers[GetCommandBufferIdx()], 
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelines[TexturedPipelineIdx].layout, 
                          0, 1, &imageDescriptors[GetCommandBufferIdx()], 
                          0, nullptr);

  PushConstantQuadInfo quadInfo{
      {l, t, r, b},
      {mSwapchain.extent.width,
       mSwapchain.extent.height}, // Framebuffer Size.
      {mRGB[0], mRGB[1], mRGB[2], mRGB[3]},
      {{0.0f, 0.0f}, {1.f, 0.0f}, {1.f, 1.f}, {0.0f, 1.f}}};
  
  vkCmdPushConstants(mCommand.commandBuffers[GetCommandBufferIdx()], 
                     mPipelines[TexturedPipelineIdx].layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(quadInfo),
                     &quadInfo);
  vkCmdDraw(mCommand.commandBuffers[GetCommandBufferIdx()], 6, 1, 0, 0);
}

void Graphics2D::DrawTexturedRect(ImageId_t imageId, int32_t l, int32_t t, 
                                  int32_t r, int32_t b, const std::vector<vec2>& textureCoords) {
  const Image* image = GetImage(imageId);
  std::array<VkDescriptorSet, 2> imageDescriptors = GetImageDescriptors(imageId);
  if (!image || imageDescriptors[0] == VK_NULL_HANDLE) return;
  if (textureCoords.size() < 4) return;
  
  // Bind the Textured-Colored Pipeline. 
  vkCmdBindPipeline(mCommand.commandBuffers[GetCommandBufferIdx()], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[TexturedPipelineIdx].pipeline);

  // Set Viewport Transform
  vkCmdSetViewport(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultViewport); 
  
  // Set Scissor
  if (mScissorIsSet) {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mSetScissor);
  } else {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultScissor);
  }

  if (mImageDescriptorsUpdated.find(imageId) == mImageDescriptorsUpdated.end()) {  
    UpdateDescriptorSet(imageDescriptors[GetCommandBufferIdx()],
                        image->GetSampler(), 
                        image->GetImageView(), 
                        image->GetImageLayout());
    mImageDescriptorsUpdated.insert(imageId);
  }

 vkCmdBindDescriptorSets(mCommand.commandBuffers[GetCommandBufferIdx()], 
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelines[TexturedPipelineIdx].layout, 0/*First Set*/, 
                          1 /*NumDescriptorSets*/, 
                          &imageDescriptors[GetCommandBufferIdx()], 
                          0, nullptr);

  PushConstantQuadInfo quadInfo{
      {l, t, r, b},
      {mSwapchain.extent.width,
       mSwapchain.extent.height}, // Framebuffer Size.
      {mRGB[0], mRGB[1], mRGB[2], mRGB[3]},
      {textureCoords[0], textureCoords[1], textureCoords[2], textureCoords[3]}};
  vkCmdPushConstants(mCommand.commandBuffers[GetCommandBufferIdx()], 
                     mPipelines[TexturedPipelineIdx].layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(quadInfo),
                     &quadInfo);
  vkCmdDraw(mCommand.commandBuffers[GetCommandBufferIdx()], 6, 1, 0, 0);
}

void Graphics2D::DrawText(FontId_t fontId, const std::string& str, 
                          int32_t baselineX, int32_t baselineY) {
  std::array<VkDescriptorSet, 2> fontDescriptors = GetFontDescriptors(fontId);
  // Bind the Text Pipeline. 
  vkCmdBindPipeline(mCommand.commandBuffers[GetCommandBufferIdx()], 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    mPipelines[TextPipelineIdx].pipeline);

  // Set Viewport Transform
  vkCmdSetViewport(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultViewport); 
  
  // Set Scissor
  if (mScissorIsSet) {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mSetScissor);
  } else {
    vkCmdSetScissor(mCommand.commandBuffers[GetCommandBufferIdx()], 0, 1, &mDefaultScissor);
  }
  
  if (mFontDescriptorsUpdated.find(fontId) == mFontDescriptorsUpdated.end()) {
    // Write Text Atlas to Descriptor Set
    UpdateDescriptorSet(fontDescriptors[GetCommandBufferIdx()],
                      mText.GetFontInfo(fontId)->fontAtlas.GetSampler(), 
                      mText.GetFontInfo(fontId)->fontAtlas.GetImageView(), 
                      mText.GetFontInfo(fontId)->fontAtlas.GetImageLayout());
    mFontDescriptorsUpdated.insert(fontId);
 }

  // Bind Descriptor Set to Text Pipeline
  vkCmdBindDescriptorSets(mCommand.commandBuffers[GetCommandBufferIdx()], 
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mPipelines[TextPipelineIdx].layout, 
                          0, 1, &fontDescriptors[GetCommandBufferIdx()], 
                          0, nullptr);

  int l = baselineX;
  const GlyphMap2& glyphData = mText.GetFontInfo(fontId)->glyphMap;
  int atlasWidth = mText.GetFontInfo(fontId)->atlasWidth;
  int atlasHeight = mText.GetFontInfo(fontId)->atlasHeight;
  for (int i=0; i<str.length(); i++) {
    char c = str[i];
    PushConstantQuadInfo quadInfo{};
    quadInfo.frameBufferSize[0] = mSwapchain.extent.width;
    quadInfo.frameBufferSize[1] = mSwapchain.extent.height;
    quadInfo.rgb[0] = mRGB[0], quadInfo.rgb[1] = mRGB[1];
    quadInfo.rgb[2] = mRGB[2], quadInfo.rgb[3] = mRGB[3];
    GlyphMap2::const_iterator data = glyphData.find(c); 
    if (data != glyphData.end()) {
      int width = data->second.width;
      int height = data->second.height;
      int x = data->second.startX;
      int y = data->second.startY;
      int glyphBaselineOffset = data->second.baselineYOffset;
      int horizontalAdvance = data->second.horizontalAdvance;
      
      quadInfo.QuadCoords[0] = l; // Increases on each iteration.
      quadInfo.QuadCoords[1] = baselineY - glyphBaselineOffset; // top. Unique per glyph
      quadInfo.QuadCoords[2] = l + width; // right. Unique per glyph. Not from parameter of method.
      quadInfo.QuadCoords[3] = quadInfo.QuadCoords[1] + height;

      // Top Left
      quadInfo.textureCoords[0].x = static_cast<float>(x) / atlasWidth;
      quadInfo.textureCoords[0].y = static_cast<float>(y) / atlasWidth;
      // Top Right
      quadInfo.textureCoords[1].x = static_cast<float>(x + width) / atlasWidth;
      quadInfo.textureCoords[1].y = static_cast<float>(y) / atlasHeight;
      // Bottom Right
      quadInfo.textureCoords[2].x = static_cast<float>(x + width) / atlasWidth;
      quadInfo.textureCoords[2].y = static_cast<float>(y + height) / atlasHeight;
      // Bottom Left
      quadInfo.textureCoords[3].x = static_cast<float>(x) / atlasWidth;
      quadInfo.textureCoords[3].y = static_cast<float>(y + height) / atlasHeight;

      vkCmdPushConstants(mCommand.commandBuffers[GetCommandBufferIdx()], 
                         mPipelines[TextPipelineIdx].layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(quadInfo),
                     &quadInfo);
      vkCmdDraw(mCommand.commandBuffers[GetCommandBufferIdx()], 6, 1, 0, 0);
      l += horizontalAdvance; // Increase l by horizontal advance glyph metric.
    }
  } 
}

void Graphics2D::UpdateDescriptorSet(VkDescriptorSet descriptorSet, VkSampler sampler, 
                         VkImageView imageView, VkImageLayout imageLayout) {
  VkWriteDescriptorSet writeDescriptors{};
  writeDescriptors.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptors.dstSet = descriptorSet;
  writeDescriptors.dstBinding = 0;
  writeDescriptors.dstArrayElement = 0;
  writeDescriptors.descriptorCount = 1;
  writeDescriptors.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

  VkDescriptorImageInfo desImageSamplerInfo{};
  desImageSamplerInfo.sampler = sampler;
  desImageSamplerInfo.imageView = imageView;
  desImageSamplerInfo.imageLayout = imageLayout;
  writeDescriptors.pImageInfo = &desImageSamplerInfo;

  vkUpdateDescriptorSets(mDevice.logicalDevice, 1, &writeDescriptors, 0, nullptr);
}

//
// DRAW STATE
//

void Graphics2D::SetColor(float r, float g, float b, float a) {
    mRGB[0] = r;
    mRGB[1] = g;
    mRGB[2] = b;
    mRGB[3] = a;
}

void Graphics2D::SetLineWidth(float lineWidth) {
  mLineWidthIsSet = true;
  mCurrentLineWidth = lineWidth;
}

void Graphics2D::UnsetLineWidth() {
  mLineWidthIsSet = false;
}

static bool Intersects(const Rect& rect1, const Rect& rect2) {
  // Calculate the right and bottom edges of the rectangles
  int rect1Right = rect1.offsetX + rect1.sizeX;
  int rect1Bottom = rect1.offsetY + rect1.sizeY;
  
  int rect2Right = rect2.offsetX + rect2.sizeX;
  int rect2Bottom = rect2.offsetY + rect2.sizeY;

  bool xOverlap = (rect1.offsetX < rect2Right) && (rect1Right > rect2.offsetX);
  bool yOverlap = (rect1.offsetY < rect2Bottom) && (rect1Bottom > rect2.offsetY);

  // The rectangles overlap if both the X and Y axes overlap
  return xOverlap && yOverlap;
}

void Graphics2D::SetScissor(const Rect& scissor, Rect& originalScissor) {
  SetScissor(scissor.offsetX, scissor.offsetY, 
             scissor.sizeX, scissor.sizeY, originalScissor);
}
 
void Graphics2D::SetScissor(int offsetX, int offsetY, 
                int sizeX, int sizeY, Rect& originalScissor) {
  offsetX = std::clamp<int>(offsetX, 0, mSwapchain.extent.width);
  offsetY = std::clamp<int>(offsetY, 0, mSwapchain.extent.height);
  sizeX = std::clamp<int>(sizeX, 0, mSwapchain.extent.width);
  sizeY = std::clamp<int>(sizeY, 0, mSwapchain.extent.height);

  if (mScissorIsSet) {
    Rect rect;
    rect.offsetX = offsetX, rect.offsetY = offsetY;
    rect.sizeX = sizeX, rect.sizeY = sizeY;

    Rect setRect;
    setRect.offsetX = mSetScissor.offset.x, setRect.offsetY = mSetScissor.offset.y;
    setRect.sizeX = mSetScissor.extent.width, setRect.sizeY = mSetScissor.extent.height;

    if (Intersects(rect, setRect)) {
      originalScissor = setRect;
      mSetScissor.offset.x = std::max<int>(rect.offsetX, setRect.offsetX);
      mSetScissor.offset.y = std::max<int>(rect.offsetY, setRect.offsetY);
      int right = std::min<int>(rect.offsetX + rect.sizeX, setRect.offsetX + setRect.sizeX);
      int bottom = std::min<int>(rect.offsetY + rect.sizeY, setRect.offsetY + setRect.sizeY);
      mSetScissor.extent.width = right - mSetScissor.offset.x; 
      mSetScissor.extent.height = bottom - mSetScissor.offset.y;
    }
  } else {
    originalScissor.offsetX = mDefaultScissor.offset.x;
    originalScissor.offsetY = mDefaultScissor.offset.y;
    originalScissor.sizeX = mDefaultScissor.extent.width;
    originalScissor.sizeY = mDefaultScissor.extent.height;
    mSetScissor.offset.x = offsetX; 
    mSetScissor.offset.y = offsetY;
    mSetScissor.extent.width = sizeX;
    mSetScissor.extent.height = sizeY;
  }
  mScissorIsSet = true;
}

void Graphics2D::UnsetScissor() {
  mScissorIsSet = false;
}

//
// RESOURCE CREATION
//

ImageId_t Graphics2D::AddImage(const std::string& imageFile) {
  static int imageIds = 1;
  if (mImageMap.find(imageFile) != mImageMap.end()) {
    return mImageMap[imageFile]; // Return Existing Image Resource
  }
  // Allocate New Image Resource
  ImageId_t id = imageIds++;
  Image newImg(mDevice.physicalDevice, mDevice.logicalDevice, imageFile);
  TemporaryCommandBuffer tempCB(mDevice.logicalDevice, mCommand.commandPool);
  newImg.Load(mDevice.queue, tempCB.commandBuffer);
  mImageResources[id] = std::move(newImg);
  mImageDescriptors.insert({id, CreateTexturedDescriptorSets()});
  mImageMap[imageFile] = id;
  return id;
}

ImageInfo Graphics2D::GetImageInfo(ImageId_t id) const {
  ImageInfo info{};
  auto iter = mImageResources.find(id);
  if (iter == mImageResources.end()) return info;
  info = iter->second.GetImageInfo();
  return info;
}

FontId_t Graphics2D::AddFont(const std::string& fontFile, const uint32_t pointSize) {
  TemporaryCommandBuffer tempCB(mDevice.logicalDevice, mCommand.commandPool);
  // Loads Font Atlas onto GPU 
  FontId_t id = mText.AddFont(fontFile, pointSize, mDevice.queue, tempCB.commandBuffer);
  mFontDescriptors.insert({id, CreateFontDescriptorSets()});
  return id;
}

const FontInfo* Graphics2D::GetFontInfo(FontId_t fontId) {
  return mText.GetFontInfo(fontId);
}

// Vulkan Resource Creation

void Graphics2D::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats,
                    VkSurfaceFormatKHR &formatOut) const {
  for (auto& format : availableFormats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      formatOut = format;
      return;
    }
  }
  throw std::runtime_error("Failed to find acceptable format from surface");
}

bool Graphics2D::CreateResources() {
  // ChoosePhysicalDeviceAndQueueFamily(); // TODO: Need to implement this eventually.
  //  XXX: These are hardcoded for now.
  SetPhysicalDeviceIdx(0);
  SetQueueFamilyIdx(0);

  uint32_t physicalDeviceCount;
  VULKAN_CALL_CHECK(vkEnumeratePhysicalDevices(
      mVulkanInstance, &physicalDeviceCount, nullptr));

  std::vector<VkPhysicalDevice> physicalDeviceList(physicalDeviceCount);
  VULKAN_CALL_CHECK(vkEnumeratePhysicalDevices(
      mVulkanInstance, &physicalDeviceCount, physicalDeviceList.data()));
  mDevice.physicalDevice = physicalDeviceList[GetPhysicalDeviceIdx()];

  // Create Logical Device and Queue
  // Using Chosen Physical Device
  VkDeviceQueueCreateInfo queueInfo{};
  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.pNext = nullptr;
  queueInfo.flags = 0;
  queueInfo.queueFamilyIndex = GetQueueFamilyIdx();
  queueInfo.queueCount = 1;
  float queuePriority = 1.f;
  queueInfo.pQueuePriorities = &queuePriority;

  VkDeviceCreateInfo deviceCi{};
  deviceCi.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCi.pNext = nullptr;
  deviceCi.queueCreateInfoCount = 1;
  deviceCi.pQueueCreateInfos = &queueInfo;
  deviceCi.enabledExtensionCount = enabledDeviceExtensions.size();
  deviceCi.ppEnabledExtensionNames = enabledDeviceExtensions.data();

  VULKAN_CALL_CHECK(vkCreateDevice(mDevice.physicalDevice, &deviceCi,
                                   nullptr, &mDevice.logicalDevice));
  vkGetDeviceQueue(mDevice.logicalDevice, GetQueueFamilyIdx(), 0, &mDevice.queue);
  
  mText.SetPhysicalDevice(mDevice.physicalDevice);
  mText.SetDevice(mDevice.logicalDevice);
  
  CreateSwapchain();
  CreateCommandBuffer(); 

  CreateRenderPass();
  CreateFramebuffers();

  // Graphics Pipelines 
  CreateLinePipeline();
  CreateFlatColoredPipeline();
  CreateTexturedPipeline();
  CreateTextPipeline();
  
  CreateDescriptorPool(); 

  // Synchronization
  CreateSyncPrimitives();
  return true;
}

bool Graphics2D::CreateSwapchain() {
  // Surface Queries

  // Surface Capabilites
  VULKAN_CALL_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      mDevice.physicalDevice, mSurface.surface, &mSurface.capabilities));

  // Surface Formats
  uint32_t numberOfSurfaceFormats = 0;
  VULKAN_CALL_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      mDevice.physicalDevice, mSurface.surface, &numberOfSurfaceFormats,
      nullptr));

  std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(
      numberOfSurfaceFormats);
  VULKAN_CALL_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      mDevice.physicalDevice, mSurface.surface, &numberOfSurfaceFormats,
      availableSurfaceFormats.data()));

  ChooseSurfaceFormat(availableSurfaceFormats, mSurface.surfaceFormat);

  mSwapchain.format = mSurface.surfaceFormat.format;
  mSwapchain.extent = mSurface.capabilities.currentExtent;

  mDefaultViewport = {
    .x = 0.0f, 
    .y = 0.0f,
    .minDepth = 0.f,
    .maxDepth = 1.f
  };
  mDefaultViewport.width = static_cast<float>(mSwapchain.extent.width);
  mDefaultViewport.height= static_cast<float>(mSwapchain.extent.height);
 
  mDefaultScissor = {
      .offset = {0, 0},
      .extent = mSwapchain.extent
  };

  VkSwapchainCreateInfoKHR swapCi{};
  swapCi.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapCi.pNext = nullptr;
  swapCi.flags = 0;
  swapCi.surface = mSurface.surface;
  swapCi.minImageCount = mSurface.capabilities.minImageCount + 1;
  swapCi.imageFormat = mSurface.surfaceFormat.format;
  swapCi.imageColorSpace = mSurface.surfaceFormat.colorSpace;
  swapCi.imageExtent = mSurface.capabilities.currentExtent;
  swapCi.imageArrayLayers = 1;
  swapCi.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapCi.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapCi.queueFamilyIndexCount = 0;
  swapCi.preTransform =
      mSurface.capabilities.currentTransform; 
  swapCi.compositeAlpha =
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapCi.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapCi.clipped = VK_TRUE;
  swapCi.oldSwapchain = VK_NULL_HANDLE;

  VULKAN_CALL_CHECK(vkCreateSwapchainKHR(mDevice.logicalDevice, &swapCi,
                                         nullptr, &mSwapchain.swapchain));

  // Get Images
  uint32_t swapImageCount;
  VULKAN_CALL_CHECK(vkGetSwapchainImagesKHR(
      mDevice.logicalDevice, mSwapchain.swapchain, &swapImageCount, nullptr));
  mSwapchain.images.resize(swapImageCount);
  VULKAN_CALL_CHECK(
      vkGetSwapchainImagesKHR(mDevice.logicalDevice, mSwapchain.swapchain,
                              &swapImageCount, mSwapchain.images.data()));

  mSwapchain.imageViews.resize(swapImageCount);
  for (int i = 0; i < swapImageCount; i++) {
    VkImageViewCreateInfo imageCi{};
    imageCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageCi.pNext = nullptr;
    imageCi.flags = 0;
    imageCi.image = mSwapchain.images[i];
    imageCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageCi.format = mSwapchain.format;
    imageCi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageCi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageCi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageCi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageCi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCi.subresourceRange.baseMipLevel = 0;
    imageCi.subresourceRange.levelCount = 1;
    imageCi.subresourceRange.baseArrayLayer = 0;
    imageCi.subresourceRange.layerCount = 1;
    VULKAN_CALL_CHECK(vkCreateImageView(mDevice.logicalDevice, &imageCi,
                                        nullptr, &mSwapchain.imageViews[i]));
  }
  return true;
}

bool Graphics2D::CreateCommandBuffer() {
  VkCommandPoolCreateInfo poolCreateInfo{};
  poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolCreateInfo.pNext = nullptr;
  poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolCreateInfo.queueFamilyIndex = GetQueueFamilyIdx();

  VULKAN_CALL_CHECK(vkCreateCommandPool(mDevice.logicalDevice,
                                        &poolCreateInfo, nullptr,
                                        &mCommand.commandPool));
  
  mCommand.commandBuffers.resize(kFRAMES_IN_FLIGHT);
  VkCommandBufferAllocateInfo cbAllocInfo;
  cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAllocInfo.pNext = nullptr;
  cbAllocInfo.commandPool = mCommand.commandPool;
  cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAllocInfo.commandBufferCount = kFRAMES_IN_FLIGHT;

  VULKAN_CALL_CHECK(vkAllocateCommandBuffers(
      mDevice.logicalDevice, &cbAllocInfo, mCommand.commandBuffers.data()));
  return true;
}

bool Graphics2D::CreateRenderPass() {
  // 1 Color Attachment
  VkAttachmentDescription colorOutAttachment{};
  colorOutAttachment.flags = 0;
  colorOutAttachment.format =
      VK_FORMAT_B8G8R8A8_SRGB;
  colorOutAttachment.samples =
      VK_SAMPLE_COUNT_1_BIT;
  colorOutAttachment.loadOp =
      VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear the attachment upon loading
  colorOutAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store
  colorOutAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorOutAttachment.finalLayout =
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Transition Image for Presentation

  // Subpass
  VkAttachmentReference colorAttachRef{};
  colorAttachRef.attachment = 0;
  colorAttachRef.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Transitions Image

  VkSubpassDescription subpassDesc{};
  subpassDesc.flags = 0;
  subpassDesc.pipelineBindPoint =
      VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDesc.inputAttachmentCount = 0;
  subpassDesc.colorAttachmentCount = 1;
  subpassDesc.pColorAttachments =
      &colorAttachRef;                       // A Single Color Attachment
  subpassDesc.pResolveAttachments = nullptr; // No Resolve Attachments
  subpassDesc.pDepthStencilAttachment =
      nullptr; // No Depth, Stencil Attachments

  VkSubpassDependency subpassDepen{};

  VkRenderPassCreateInfo rPassCreateInfo{};
  rPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rPassCreateInfo.pNext = nullptr;
  rPassCreateInfo.flags = 0;
  rPassCreateInfo.attachmentCount = 1;
  rPassCreateInfo.pAttachments = &colorOutAttachment;
  rPassCreateInfo.subpassCount = 1;
  rPassCreateInfo.pSubpasses = &subpassDesc;
  rPassCreateInfo.dependencyCount = 0;
  rPassCreateInfo.pDependencies = &subpassDepen;

  VULKAN_CALL_CHECK(vkCreateRenderPass(mDevice.logicalDevice,
                                       &rPassCreateInfo, nullptr,
                                       &mRendering.renderPass));
  return true;
}

bool Graphics2D::CreateFramebuffers() {
  mSwapchain.framebuffers.resize(mSwapchain.images.size());
  for (int i = 0; i < mSwapchain.framebuffers.size(); i++) {
    VkFramebufferCreateInfo fbCi{};
    fbCi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCi.pNext = nullptr;
    fbCi.flags = 0;
    fbCi.renderPass = mRendering.renderPass;
    fbCi.attachmentCount = 1;
    fbCi.pAttachments = &mSwapchain.imageViews[i];
    fbCi.width = mSwapchain.extent.width;
    fbCi.height = mSwapchain.extent.height;
    fbCi.layers = 1;
    VULKAN_CALL_CHECK(vkCreateFramebuffer(
        mDevice.logicalDevice, &fbCi, nullptr, &mSwapchain.framebuffers[i]));
  }
  return true;
}


VkShaderModule Graphics2D::CreateShaderModule(VkDevice device, const char* shaderFile) {
  std::ifstream shaderStream(shaderFile, std::ios::binary);
  if (!shaderStream.is_open()) {
    throw std::runtime_error(std::string("Failed to open shader file") + shaderFile);
  }

  struct stat fileInfo;
  if (stat(shaderFile, &fileInfo) == -1) {
    throw std::runtime_error(std::string("Failed to stat() shader file") + shaderFile);
  }

  size_t codeSize = fileInfo.st_size;
  if ((codeSize % 4) != 0) {
    throw std::runtime_error("Shader file bytes NOT multiple of 4");
  }
  
  std::vector<char> shaderCode(codeSize);
  if (!shaderStream.read(shaderCode.data(), codeSize)) {
    throw std::runtime_error("Failed to read() shader file");
  }

  VkShaderModule shaderModule;
  VkShaderModuleCreateInfo shaderModuleInfo{};
  shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleInfo.pNext = nullptr;
  shaderModuleInfo.flags = 0;
  shaderModuleInfo.codeSize = codeSize;
  shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

  vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule);
  return shaderModule;
}

VkShaderModule Graphics2D::CreateShaderModule(VkDevice device, const uint8_t* shaderCode, const int byteSize) {
  if ((byteSize% 4) != 0) {
    throw std::runtime_error("Shader file bytes NOT multiple of 4");
  }
  
  VkShaderModule shaderModule;
  VkShaderModuleCreateInfo shaderModuleInfo{};
  shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleInfo.pNext = nullptr;
  shaderModuleInfo.flags = 0;
  shaderModuleInfo.codeSize = byteSize;
  shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode);

  vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule);
  return shaderModule;
}

bool Graphics2D::CreateLinePipeline() {
  // Pipeline Layout:
  //  - DescriptorSetLayout
  //  - Push Constant
  VkPipelineLayoutCreateInfo playoutCi{};
  VkPushConstantRange framebufferPushConstants;
  framebufferPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  framebufferPushConstants.offset = 0;
  framebufferPushConstants.size = sizeof(PushConstantQuadInfo);

  playoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  playoutCi.pNext = nullptr;
  playoutCi.flags = 0;
  playoutCi.setLayoutCount = 0; // No Descriptor Sets necessary for this pipeline.
  playoutCi.pSetLayouts = nullptr;
  playoutCi.pushConstantRangeCount = 1;
  playoutCi.pPushConstantRanges = &framebufferPushConstants;
  VULKAN_CALL_CHECK(vkCreatePipelineLayout(
      mDevice.logicalDevice, &playoutCi, nullptr, &mPipelines[LinePipelineIdx].layout));
  
  VkShaderModule vsModule = CreateShaderModule(
      mDevice.logicalDevice, shaderLinevs_spv, shaderLinevs_spv_len);
  VkShaderModule fsModule =
      CreateShaderModule(mDevice.logicalDevice, shaderLinefs_spv, shaderLinefs_spv_len);

  // Shader Stage 1 - Vertex Shader
  VkPipelineShaderStageCreateInfo vertexShaderInfo{};
  vertexShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderInfo.pNext = nullptr;
  vertexShaderInfo.flags = 0;
  vertexShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexShaderInfo.module = vsModule;
  vertexShaderInfo.pName = "main";
  vertexShaderInfo.pSpecializationInfo = nullptr;

  // Shader Stage 2 - Fragment Shader
  VkPipelineShaderStageCreateInfo fragmentShaderInfo{};
  fragmentShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderInfo.pNext = nullptr;
  fragmentShaderInfo.flags = 0;
  fragmentShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentShaderInfo.module = fsModule;
  fragmentShaderInfo.pName = "main";
  fragmentShaderInfo.pSpecializationInfo = nullptr;

  std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderInfo = {
      vertexShaderInfo, fragmentShaderInfo};

  // No Vertex Buffers for now.
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAsInfo{};
  inputAsInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAsInfo.pNext = nullptr;
  inputAsInfo.flags = 0;
  inputAsInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  inputAsInfo.primitiveRestartEnable = VK_FALSE;

  VkPipelineTessellationStateCreateInfo tessInfo{};
  tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessInfo.pNext = nullptr;
  tessInfo.flags = 0;
  tessInfo.patchControlPoints = 0;

  VkPipelineViewportStateCreateInfo vpInfo{};
  vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vpInfo.pNext = nullptr;
  vpInfo.flags = 0;
  vpInfo.viewportCount = 1; // Dynamic Viewport
  vpInfo.scissorCount = 1; // Dynamics Scissor

  VkPipelineRasterizationStateCreateInfo rtzrInfo{};
  rtzrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rtzrInfo.pNext = nullptr;
  rtzrInfo.flags = 0;
  rtzrInfo.depthClampEnable = VK_FALSE;
  rtzrInfo.rasterizerDiscardEnable = VK_FALSE;
  rtzrInfo.polygonMode =
      VK_POLYGON_MODE_FILL;
  rtzrInfo.cullMode = VK_CULL_MODE_NONE;
  rtzrInfo.frontFace =
      VK_FRONT_FACE_CLOCKWISE;
  rtzrInfo.depthBiasEnable = VK_FALSE;
  rtzrInfo.lineWidth = mDefaultLineWidth; // Default Line Width!

  VkPipelineMultisampleStateCreateInfo msCi{};
  msCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  msCi.pNext = nullptr;
  msCi.flags = 0;
  msCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  msCi.sampleShadingEnable = VK_FALSE;
  msCi.alphaToCoverageEnable = VK_FALSE;
  msCi.alphaToOneEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colBlendCi{};

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  colBlendCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colBlendCi.pNext = nullptr;
  colBlendCi.flags = 0;
  colBlendCi.logicOpEnable = VK_FALSE;
  colBlendCi.attachmentCount = 1;
  colBlendCi.pAttachments = &colorBlendAttachment;
  
  std::array<VkDynamicState,3> dynamicState = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_LINE_WIDTH,
  };
  VkPipelineDynamicStateCreateInfo dynamicCi{};
  dynamicCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicCi.pNext = nullptr;
  dynamicCi.flags = 0;
  dynamicCi.dynamicStateCount = dynamicState.size();
  dynamicCi.pDynamicStates = dynamicState.data(); 

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.pNext = nullptr;
  pipelineCreateInfo.flags = 0;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = pipelineShaderInfo.data();
  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAsInfo;
  pipelineCreateInfo.pTessellationState = nullptr;
  pipelineCreateInfo.pViewportState = &vpInfo;
  pipelineCreateInfo.pRasterizationState = &rtzrInfo;
  pipelineCreateInfo.pMultisampleState = &msCi;
  pipelineCreateInfo.pDepthStencilState = nullptr;
  pipelineCreateInfo.pColorBlendState = &colBlendCi;
  pipelineCreateInfo.pDynamicState = &dynamicCi;
  pipelineCreateInfo.layout = mPipelines[LinePipelineIdx].layout;
  pipelineCreateInfo.renderPass = mRendering.renderPass;
  pipelineCreateInfo.subpass = 0;
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VULKAN_CALL_CHECK(vkCreateGraphicsPipelines(
      mDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
      &mPipelines[LinePipelineIdx].pipeline));
  
  vkDestroyShaderModule(mDevice.logicalDevice, vsModule, nullptr);
  vkDestroyShaderModule(mDevice.logicalDevice, fsModule, nullptr);
  return true;
}

bool Graphics2D::CreateFlatColoredPipeline() {
  // Pipeline Layout:
  //  - DescriptorSetLayout
  //  - PushConstant
  VkPipelineLayoutCreateInfo playoutCi{};
  VkPushConstantRange framebufferPushConstants;
  framebufferPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  framebufferPushConstants.offset = 0;
  framebufferPushConstants.size = sizeof(PushConstantQuadInfo);

   playoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   playoutCi.pNext = nullptr;
   playoutCi.flags = 0;
   playoutCi.setLayoutCount = 0; // No Descriptor Sets
   playoutCi.pSetLayouts = nullptr;
   playoutCi.pushConstantRangeCount = 1;
   playoutCi.pPushConstantRanges = &framebufferPushConstants;
   VULKAN_CALL_CHECK(vkCreatePipelineLayout(
       mDevice.logicalDevice, &playoutCi, nullptr, &mPipelines[FlatPipelineIdx].layout));

   VkShaderModule vsModule = CreateShaderModule(
       mDevice.logicalDevice, shaderFlatvs_spv, shaderFlatvs_spv_len);
   VkShaderModule fsModule =
       CreateShaderModule(mDevice.logicalDevice, shaderFlatfs_spv, shaderFlatfs_spv_len);

   // Shader Stage 1 - Vertex Shader
   VkPipelineShaderStageCreateInfo vertexShaderInfo{};
   vertexShaderInfo.sType =
       VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   vertexShaderInfo.pNext = nullptr;
   vertexShaderInfo.flags = 0;
   vertexShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   vertexShaderInfo.module = vsModule;
   vertexShaderInfo.pName = "main";
   vertexShaderInfo.pSpecializationInfo = nullptr;

   // Shader Stage 1 - Fragment Shader
   VkPipelineShaderStageCreateInfo fragmentShaderInfo{};
   fragmentShaderInfo.sType =
       VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   fragmentShaderInfo.pNext = nullptr;
   fragmentShaderInfo.flags = 0;
   fragmentShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   fragmentShaderInfo.module = fsModule;
   fragmentShaderInfo.pName = "main";
   fragmentShaderInfo.pSpecializationInfo = nullptr;

   std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderInfo = {
       vertexShaderInfo, fragmentShaderInfo};

   // No Vertex Buffers for now.
   VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
   vertexInputInfo.sType =
       VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.pNext = nullptr;
   vertexInputInfo.flags = 0;
   vertexInputInfo.vertexBindingDescriptionCount = 0;
   vertexInputInfo.vertexAttributeDescriptionCount = 0;

   VkPipelineInputAssemblyStateCreateInfo inputAsInfo{};
   inputAsInfo.sType =
       VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAsInfo.pNext = nullptr;
   inputAsInfo.flags = 0;
   inputAsInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAsInfo.primitiveRestartEnable = VK_FALSE;

   VkPipelineTessellationStateCreateInfo tessInfo{};
   tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
   tessInfo.pNext = nullptr;
   tessInfo.flags = 0;
   tessInfo.patchControlPoints = 0;

   VkPipelineViewportStateCreateInfo vpInfo{};
   vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   vpInfo.pNext = nullptr;
   vpInfo.flags = 0;
   vpInfo.viewportCount = 1; // Set dynamically
   vpInfo.scissorCount = 1; // Set dynamically

   VkPipelineRasterizationStateCreateInfo rtzrInfo{};
   rtzrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rtzrInfo.pNext = nullptr;
   rtzrInfo.flags = 0;
   rtzrInfo.depthClampEnable = VK_FALSE;
   rtzrInfo.rasterizerDiscardEnable = VK_FALSE;
   rtzrInfo.polygonMode =
       VK_POLYGON_MODE_FILL;
   rtzrInfo.cullMode = VK_CULL_MODE_NONE;
   rtzrInfo.frontFace =
       VK_FRONT_FACE_CLOCKWISE;
   rtzrInfo.depthBiasEnable = VK_FALSE;
   rtzrInfo.lineWidth = 1.0f; 

   VkPipelineMultisampleStateCreateInfo msCi{};
   msCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   msCi.pNext = nullptr;
   msCi.flags = 0;
   msCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   msCi.sampleShadingEnable = VK_FALSE;
   msCi.alphaToCoverageEnable = VK_FALSE;
   msCi.alphaToOneEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colBlendCi{};

   VkPipelineColorBlendAttachmentState colorBlendAttachment{};
   colorBlendAttachment.colorWriteMask =
       VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;
   colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
   colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
   colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
   colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
   colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
   colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

   colBlendCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   colBlendCi.pNext = nullptr;
   colBlendCi.flags = 0;
   colBlendCi.logicOpEnable = VK_FALSE;
   colBlendCi.attachmentCount = 1;
   colBlendCi.pAttachments = &colorBlendAttachment;

   std::array<VkDynamicState, 2> dynamicState = {
     VK_DYNAMIC_STATE_VIEWPORT,
     VK_DYNAMIC_STATE_SCISSOR
   };

   VkPipelineDynamicStateCreateInfo dynamicCi{};
   dynamicCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicCi.pNext = nullptr;
   dynamicCi.flags = 0;
   dynamicCi.dynamicStateCount = dynamicState.size();
   dynamicCi.pDynamicStates = dynamicState.data();

   VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
   pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipelineCreateInfo.pNext = nullptr;
   pipelineCreateInfo.flags = 0;
   pipelineCreateInfo.stageCount = 2;
   pipelineCreateInfo.pStages = pipelineShaderInfo.data();
   pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
   pipelineCreateInfo.pInputAssemblyState = &inputAsInfo;
   pipelineCreateInfo.pTessellationState = nullptr;
   pipelineCreateInfo.pViewportState = &vpInfo;
   pipelineCreateInfo.pRasterizationState = &rtzrInfo;
   pipelineCreateInfo.pMultisampleState = &msCi;
   pipelineCreateInfo.pDepthStencilState = nullptr;
   pipelineCreateInfo.pColorBlendState = &colBlendCi;
   pipelineCreateInfo.pDynamicState = &dynamicCi;
   pipelineCreateInfo.layout = mPipelines[FlatPipelineIdx].layout;
   pipelineCreateInfo.renderPass = mRendering.renderPass;
   pipelineCreateInfo.subpass = 0;
   pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

   VULKAN_CALL_CHECK(vkCreateGraphicsPipelines(
       mDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
       &mPipelines[FlatPipelineIdx].pipeline));
   vkDestroyShaderModule(mDevice.logicalDevice, vsModule, nullptr);
   vkDestroyShaderModule(mDevice.logicalDevice, fsModule, nullptr);
   return true;
}

bool Graphics2D::CreateTexturedPipeline() {
  // Pipeline Layout:
  //  - DescriptorSetLayout
  //  - PushConstant
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutCi{};
  layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutCi.flags = 0;
  layoutCi.bindingCount = 1;
  layoutCi.pBindings = &layoutBinding;
  
  vkCreateDescriptorSetLayout(mDevice.logicalDevice, &layoutCi, 
                              nullptr, &mPipelines[TexturedPipelineIdx].descriptorLayouts); 

  VkPipelineLayoutCreateInfo playoutCi{};
  VkPushConstantRange framebufferPushConstants;
  framebufferPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  framebufferPushConstants.offset = 0;
  framebufferPushConstants.size = sizeof(PushConstantQuadInfo);

  playoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  playoutCi.pNext = nullptr;
  playoutCi.flags = 0;
  playoutCi.setLayoutCount = 1;
  playoutCi.pSetLayouts = &mPipelines[TexturedPipelineIdx].descriptorLayouts;
  playoutCi.pushConstantRangeCount = 1;
  playoutCi.pPushConstantRanges = &framebufferPushConstants;
  VULKAN_CALL_CHECK(vkCreatePipelineLayout(
      mDevice.logicalDevice, &playoutCi, nullptr,
      &mPipelines[TexturedPipelineIdx].layout));
  
  VkShaderModule vsModule = CreateShaderModule(
      mDevice.logicalDevice, shaderTexturedvs_spv, shaderTexturedvs_spv_len);
  VkShaderModule fsModule =
      CreateShaderModule(mDevice.logicalDevice, shaderTexturedfs_spv, 
                         shaderTexturedfs_spv_len);

  // Shader Stage 1 - Vertex Shader
  VkPipelineShaderStageCreateInfo vertexShaderInfo{};
  vertexShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderInfo.pNext = nullptr;
  vertexShaderInfo.flags = 0;
  vertexShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexShaderInfo.module = vsModule;
  vertexShaderInfo.pName = "main";
  vertexShaderInfo.pSpecializationInfo = nullptr;
  

  // Shader Stage 2 - Fragment Shader
  VkPipelineShaderStageCreateInfo fragmentShaderInfo{};
  fragmentShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderInfo.pNext = nullptr;
  fragmentShaderInfo.flags = 0;
  fragmentShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentShaderInfo.module = fsModule;
  fragmentShaderInfo.pName = "main";
  fragmentShaderInfo.pSpecializationInfo = nullptr;

  std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderInfo = {
      vertexShaderInfo, fragmentShaderInfo};

  // No Vertex Buffers for now.
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAsInfo{};
  inputAsInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAsInfo.pNext = nullptr;
  inputAsInfo.flags = 0;
  inputAsInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAsInfo.primitiveRestartEnable = VK_FALSE;

  VkPipelineTessellationStateCreateInfo tessInfo{};
  tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessInfo.pNext = nullptr;
  tessInfo.flags = 0;
  tessInfo.patchControlPoints = 0;

  VkPipelineViewportStateCreateInfo vpInfo{};
  vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vpInfo.pNext = nullptr;
  vpInfo.flags = 0;
  vpInfo.viewportCount = 1; // Set Dynamically
  vpInfo.scissorCount = 1; // Set Dynamically

  VkPipelineRasterizationStateCreateInfo rtzrInfo{};
  rtzrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rtzrInfo.pNext = nullptr;
  rtzrInfo.flags = 0;
  rtzrInfo.depthClampEnable = VK_FALSE;
  rtzrInfo.rasterizerDiscardEnable = VK_FALSE;
  rtzrInfo.polygonMode =
      VK_POLYGON_MODE_FILL;
  rtzrInfo.cullMode = VK_CULL_MODE_NONE;
  rtzrInfo.frontFace =
      VK_FRONT_FACE_CLOCKWISE;
  rtzrInfo.depthBiasEnable = VK_FALSE;
  rtzrInfo.lineWidth = 1.0f; 

  VkPipelineMultisampleStateCreateInfo msCi{};
  msCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  msCi.pNext = nullptr;
  msCi.flags = 0;
  msCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  msCi.sampleShadingEnable = VK_FALSE;
  msCi.alphaToCoverageEnable = VK_FALSE;
  msCi.alphaToOneEnable = VK_FALSE;
  
  // Pipeline Blending State
  VkPipelineColorBlendStateCreateInfo colBlendCi{};
  
  // Blending for a specific attachment
  // of render pass.
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.blendEnable = VK_TRUE; // Enable blending
    // Blend Factors and Operation for RGB
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; 
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; 
    // Blend Factors and Operation for Alpha
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    // Specify which components are available for writing.
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  
  colBlendCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colBlendCi.pNext = nullptr;
  colBlendCi.flags = 0;
  colBlendCi.logicOpEnable = VK_FALSE;
  colBlendCi.attachmentCount = 1;
  colBlendCi.pAttachments = &colorBlendAttachment;

  std::array<VkDynamicState, 2> dynamicState = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicCi{};
  dynamicCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicCi.pNext = nullptr;
  dynamicCi.flags = 0;
  dynamicCi.dynamicStateCount = dynamicState.size();
  dynamicCi.pDynamicStates = dynamicState.data();

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.pNext = nullptr;
  pipelineCreateInfo.flags = 0;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = pipelineShaderInfo.data();
  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAsInfo;
  pipelineCreateInfo.pTessellationState = nullptr;
  pipelineCreateInfo.pViewportState = &vpInfo;
  pipelineCreateInfo.pRasterizationState = &rtzrInfo;
  pipelineCreateInfo.pMultisampleState = &msCi;
  pipelineCreateInfo.pDepthStencilState = nullptr;
  pipelineCreateInfo.pColorBlendState = &colBlendCi;
  pipelineCreateInfo.pDynamicState = &dynamicCi;
  pipelineCreateInfo.layout = mPipelines[TexturedPipelineIdx].layout;
  pipelineCreateInfo.renderPass = mRendering.renderPass;
  pipelineCreateInfo.subpass = 0;
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VULKAN_CALL_CHECK(vkCreateGraphicsPipelines(
      mDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
      &mPipelines[TexturedPipelineIdx].pipeline));
   vkDestroyShaderModule(mDevice.logicalDevice, vsModule, nullptr);
   vkDestroyShaderModule(mDevice.logicalDevice, fsModule, nullptr);
  return true;
}

bool Graphics2D::CreateTextPipeline() {
  // Pipeline Layout:
  //  - DescriptorSetLayout
  //  - PushConstant
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutCi{};
  layoutCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutCi.flags = 0;
  layoutCi.bindingCount = 1;
  layoutCi.pBindings = &layoutBinding;
  
  vkCreateDescriptorSetLayout(mDevice.logicalDevice, &layoutCi, 
                              nullptr, &mPipelines[TextPipelineIdx].descriptorLayouts);

  VkPipelineLayoutCreateInfo playoutCi{};
  VkPushConstantRange framebufferPushConstants;
  framebufferPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  framebufferPushConstants.offset = 0;
  framebufferPushConstants.size = sizeof(PushConstantQuadInfo);

  playoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  playoutCi.pNext = nullptr;
  playoutCi.flags = 0;
  playoutCi.setLayoutCount = 1;
  playoutCi.pSetLayouts = &mPipelines[TextPipelineIdx].descriptorLayouts; // For Text Atlas
  playoutCi.pushConstantRangeCount = 1;
  playoutCi.pPushConstantRanges = &framebufferPushConstants;
  VULKAN_CALL_CHECK(vkCreatePipelineLayout(
      mDevice.logicalDevice, &playoutCi, nullptr, &mPipelines[TextPipelineIdx].layout));

  VkShaderModule vsModule = CreateShaderModule(
      mDevice.logicalDevice, shaderTexturedvs_spv, shaderTexturedvs_spv_len);
  VkShaderModule fsModule =
      CreateShaderModule(mDevice.logicalDevice, shaderTextfs_spv, shaderTextfs_spv_len);

  // Shader Stage 1 - Vertex Shader
  VkPipelineShaderStageCreateInfo vertexShaderInfo{};
  vertexShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderInfo.pNext = nullptr;
  vertexShaderInfo.flags = 0;
  vertexShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexShaderInfo.module = vsModule;
  vertexShaderInfo.pName = "main";
  vertexShaderInfo.pSpecializationInfo = nullptr;

  // Shader Stage 2 - Fragment Shader
  VkPipelineShaderStageCreateInfo fragmentShaderInfo{};
  fragmentShaderInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderInfo.pNext = nullptr;
  fragmentShaderInfo.flags = 0;
  fragmentShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentShaderInfo.module = fsModule;
  fragmentShaderInfo.pName = "main";
  fragmentShaderInfo.pSpecializationInfo = nullptr;

  std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderInfo = {
      vertexShaderInfo, fragmentShaderInfo};

  // No Vertex Buffers for now.
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAsInfo{};
  inputAsInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAsInfo.pNext = nullptr;
  inputAsInfo.flags = 0;
  inputAsInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAsInfo.primitiveRestartEnable = VK_FALSE;

  VkPipelineTessellationStateCreateInfo tessInfo{};
  tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessInfo.pNext = nullptr;
  tessInfo.flags = 0;
  tessInfo.patchControlPoints = 0;
  
  VkPipelineViewportStateCreateInfo vpInfo{};
  vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vpInfo.pNext = nullptr;
  vpInfo.flags = 0;
  vpInfo.viewportCount = 1; // Set Dynamically
  vpInfo.scissorCount = 1; // Set Dynamically

  VkPipelineRasterizationStateCreateInfo rtzrInfo{};
  rtzrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rtzrInfo.pNext = nullptr;
  rtzrInfo.flags = 0;
  rtzrInfo.depthClampEnable = VK_FALSE;
  rtzrInfo.rasterizerDiscardEnable = VK_FALSE;
  rtzrInfo.polygonMode =
      VK_POLYGON_MODE_FILL;
  rtzrInfo.cullMode = VK_CULL_MODE_NONE;
  rtzrInfo.frontFace =
      VK_FRONT_FACE_CLOCKWISE;
  rtzrInfo.depthBiasEnable = VK_FALSE;
  rtzrInfo.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo msCi{};
  msCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  msCi.pNext = nullptr;
  msCi.flags = 0;
  msCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  msCi.sampleShadingEnable = VK_FALSE;
  msCi.alphaToCoverageEnable = VK_FALSE;
  msCi.alphaToOneEnable = VK_FALSE;
  
  // Pipeline Blending State
  VkPipelineColorBlendStateCreateInfo colBlendCi{};
  
  // Blending for a specific attachment
  // of render pass.
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.blendEnable = VK_TRUE; // Enable blending
    // Blend Factors and Operation for RGB
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; 
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; 
    // Blend Factors and Operation for Alpha
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    // Specify which components are available for writing.
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  
  colBlendCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colBlendCi.pNext = nullptr;
  colBlendCi.flags = 0;
  colBlendCi.logicOpEnable = VK_FALSE;
  colBlendCi.attachmentCount = 1;
  colBlendCi.pAttachments = &colorBlendAttachment;
  
  std::array<VkDynamicState, 2> dynamicState = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };
  VkPipelineDynamicStateCreateInfo dynamicCi{};
  dynamicCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicCi.pNext = nullptr;
  dynamicCi.flags = 0;
  dynamicCi.dynamicStateCount = dynamicState.size();
  dynamicCi.pDynamicStates = dynamicState.data();

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.pNext = nullptr;
  pipelineCreateInfo.flags = 0;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = pipelineShaderInfo.data();
  pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
  pipelineCreateInfo.pInputAssemblyState = &inputAsInfo;
  pipelineCreateInfo.pTessellationState = nullptr;
  pipelineCreateInfo.pViewportState = &vpInfo;
  pipelineCreateInfo.pRasterizationState = &rtzrInfo;
  pipelineCreateInfo.pMultisampleState = &msCi;
  pipelineCreateInfo.pDepthStencilState = nullptr;
  pipelineCreateInfo.pColorBlendState = &colBlendCi;
  pipelineCreateInfo.pDynamicState = &dynamicCi;
  pipelineCreateInfo.layout = mPipelines[TextPipelineIdx].layout;
  pipelineCreateInfo.renderPass = mRendering.renderPass;
  pipelineCreateInfo.subpass = 0;
  pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

  VULKAN_CALL_CHECK(vkCreateGraphicsPipelines(
      mDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
      &mPipelines[TextPipelineIdx].pipeline));
  vkDestroyShaderModule(mDevice.logicalDevice, vsModule, nullptr);
  vkDestroyShaderModule(mDevice.logicalDevice, fsModule, nullptr);
  return true;
}

void Graphics2D::CreateDescriptorPool() {
  VkDescriptorPoolSize poolSize;
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = kFRAMES_IN_FLIGHT;

  VkDescriptorPoolCreateInfo poolCi{};
  poolCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolCi.flags =  0;
  poolCi.maxSets = kFRAMES_IN_FLIGHT*30; // Max number of Descriptor Sets.
                                                 // TODO: Arbitrarily chosen
                                                 //       for now. Fix Later
                                                 //       to handle allocation
                                                 //       failure.
  poolCi.poolSizeCount = 1;
  poolCi.pPoolSizes = &poolSize;

  vkCreateDescriptorPool(mDevice.logicalDevice, &poolCi, nullptr, &mDescriptors.descriptorPool);
}

std::array<VkDescriptorSet, 2> Graphics2D::CreateFontDescriptorSets() {
  std::array<VkDescriptorSet, 2> sets = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  std::array<VkDescriptorSetLayout, 2> layouts = {mPipelines[TextPipelineIdx].descriptorLayouts, mPipelines[TextPipelineIdx].descriptorLayouts};
  VkDescriptorSetAllocateInfo dsAllocInfo{}; 
  dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dsAllocInfo.pNext = nullptr;
  dsAllocInfo.descriptorPool = mDescriptors.descriptorPool;
  dsAllocInfo.descriptorSetCount = 2;
  dsAllocInfo.pSetLayouts = layouts.data();
  vkAllocateDescriptorSets(mDevice.logicalDevice, &dsAllocInfo, sets.data());
  return sets;
}

std::array<VkDescriptorSet, 2> Graphics2D::CreateTexturedDescriptorSets() {
  std::array<VkDescriptorSet, 2> sets = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  std::array<VkDescriptorSetLayout, 2> layouts = {mPipelines[TexturedPipelineIdx].descriptorLayouts, mPipelines[TexturedPipelineIdx].descriptorLayouts};
  VkDescriptorSetAllocateInfo dsAllocInfo{}; 
  dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dsAllocInfo.pNext = nullptr;
  dsAllocInfo.descriptorPool = mDescriptors.descriptorPool;
  dsAllocInfo.descriptorSetCount = 2;
  dsAllocInfo.pSetLayouts = layouts.data();
  vkAllocateDescriptorSets(mDevice.logicalDevice, &dsAllocInfo, sets.data());
  return sets;
}

bool Graphics2D::CreateSyncPrimitives() {
  mSync.imageAvailableSemaphores.resize(kFRAMES_IN_FLIGHT);
  mSync.renderCompleteSemaphores.resize(kFRAMES_IN_FLIGHT);
  mSync.commandBufferFences.resize(kFRAMES_IN_FLIGHT);
  // Create Presentation Semaphore
  
  // Don't start render pass until image is available.
  VkSemaphoreCreateInfo semCi = {.sType =
                                     VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                 .pNext = nullptr,
                                 .flags = 0};
  VkFenceCreateInfo fenceCi{};
  fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCi.pNext = nullptr;
  fenceCi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i=0; i<kFRAMES_IN_FLIGHT; i++) {

    // Don't start render pass until image is available.
    VULKAN_CALL_CHECK(vkCreateSemaphore(mDevice.logicalDevice, &semCi, nullptr,
                                        &mSync.imageAvailableSemaphores[i]));
    // Don't start presentation until render pass is complete.
    VULKAN_CALL_CHECK(vkCreateSemaphore(mDevice.logicalDevice, &semCi, nullptr,
                                        &mSync.renderCompleteSemaphores[i]));
    // Block CPU until command buffer is finished executing.
    VULKAN_CALL_CHECK(vkCreateFence(mDevice.logicalDevice, &fenceCi, nullptr,
                                    &mSync.commandBufferFences[i]));
  }
  return true;
}


// RESOURCE RETRIEVAL

const Image* Graphics2D::GetImage(ImageId_t id) const {
  auto iter = mImageResources.find(id); 
  if (iter == mImageResources.end()) return nullptr;
  return &(iter->second);
}

std::array<VkDescriptorSet, 2> Graphics2D::GetImageDescriptors(ImageId_t id) const {
  auto iter = mImageDescriptors.find(id);
  if (iter == mImageDescriptors.end()) return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  return iter->second;
}

std::array<VkDescriptorSet, 2> Graphics2D::GetFontDescriptors(FontId_t id) const {
  auto iter = mFontDescriptors.find(id);
  if (iter == mFontDescriptors.end()) return {VK_NULL_HANDLE, VK_NULL_HANDLE};
  return iter->second;
}


