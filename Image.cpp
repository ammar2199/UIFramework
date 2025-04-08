#include "Image.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <iostream>
#include <utility>

#include <vulkan/vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"

// From https://vulkan-tutorial.com/Vertex_buffers/Vertex_buffer_creation#page_Memory-requirements
uint32_t Image::FindMemoryType(uint32_t allowableMemoryTypes,
                          VkMemoryPropertyFlags requestedMemoryProperties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps); 
    for (int i=0; i<memProps.memoryTypeCount; i++) {
      if ((allowableMemoryTypes & (1 << i)) && ((memProps.memoryTypes[i].propertyFlags & requestedMemoryProperties) == requestedMemoryProperties)) {
        return i;
      }
    }
    return -1; 
}

Image::Image() : mPhysicalDevice(VK_NULL_HANDLE), mDevice(VK_NULL_HANDLE), 
                 mImage(VK_NULL_HANDLE), mImageView(VK_NULL_HANDLE), 
                 mFormat(VK_FORMAT_UNDEFINED), mDeviceMemory(VK_NULL_HANDLE), 
                 mSampler(VK_NULL_HANDLE), mImageLayout(VK_IMAGE_LAYOUT_UNDEFINED), 
                 mIsLoaded(false) {
}

Image::Image(VkPhysicalDevice phyDev, VkDevice device, 
             const std::string& file) : mFile(file), 
              mPhysicalDevice(phyDev),
              mDevice(device), mImage(VK_NULL_HANDLE), 
              mImageView(VK_NULL_HANDLE),
              mFormat(VK_FORMAT_UNDEFINED),
              mDeviceMemory(VK_NULL_HANDLE), 
              mSampler(VK_NULL_HANDLE),
              mImageLayout(VK_IMAGE_LAYOUT_UNDEFINED),
              mIsLoaded(false) {
  int res = stbi_info(file.c_str(), &mInfo.width, &mInfo.height, nullptr);
  if (res != 1) {
    throw std::runtime_error("Failed to get Image Info from: " + mFile);
  }
  mInfo.bytesPerPixel = 4;
  mFormat = VK_FORMAT_R8G8B8A8_SRGB;
  CreateImage(); 
  CreateDeviceMemory();
  vkBindImageMemory(mDevice, mImage, mDeviceMemory, 0);
  CreateImageView();
  CreateSampler();
}

Image::Image(VkPhysicalDevice phyDev, VkDevice device, const int width, 
             const int height, const int bytesPerPixel) : mPhysicalDevice(phyDev), 
             mDevice(device), mIsLoaded(false) {
  assert(width > 0 && height > 0 && bytesPerPixel > 0 && "Image dimensions must be greater than 0");
  mInfo.width = width;
  mInfo.height = height;
  mInfo.bytesPerPixel = bytesPerPixel;
  if (bytesPerPixel != 1 && bytesPerPixel != 4) {
    throw std::runtime_error("Must have 1 or 4 components in Image");
  }

  switch (bytesPerPixel) {
    case 1: 
      mFormat = VK_FORMAT_R8_UNORM;
      break;
    case 4:
      mFormat = VK_FORMAT_R8G8B8A8_SRGB;
      break;
    default:
      break;
  }
 
  CreateImage();
  CreateDeviceMemory();
  vkBindImageMemory(mDevice, mImage, mDeviceMemory, 0);
  CreateImageView();
  CreateSampler();
}

Image::~Image() {
  if (mImage == VK_NULL_HANDLE) {
    return;
  }
  
  // Destroy Resources
  vkDestroySampler(mDevice, mSampler, nullptr);
  vkDestroyImageView(mDevice, mImageView, nullptr);
  vkFreeMemory(mDevice, mDeviceMemory, nullptr);
  vkDestroyImage(mDevice, mImage, nullptr);
}

Image& Image::operator=(Image&& other) {
  mInfo = other.mInfo;
  mFile = std::move(other.mFile);
  mPhysicalDevice = other.mPhysicalDevice;
  mDevice = other.mDevice;
  mImage = other.mImage;
  mImageView = other.mImageView;
  mFormat = other.mFormat;
  mDeviceMemory = other.mDeviceMemory;
  mSampler = other.mSampler;
  mImageLayout = other.mImageLayout;
  mIsLoaded = other.mIsLoaded;

  other.mImage = VK_NULL_HANDLE;
  other.mImageView = VK_NULL_HANDLE;
  other.mDeviceMemory = VK_NULL_HANDLE;
  other.mSampler = VK_NULL_HANDLE;
  return *this;
}


void Image::CreateImage() {
  VkImageCreateInfo imgCi{};
  imgCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imgCi.imageType = VK_IMAGE_TYPE_2D;
  imgCi.extent.width = mInfo.width;
  imgCi.extent.height = mInfo.height;
  imgCi.extent.depth = 1;
  imgCi.mipLevels = 1;
  imgCi.arrayLayers = 1;
  imgCi.format = mFormat;
  imgCi.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgCi.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imgCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgCi.samples =
      VK_SAMPLE_COUNT_1_BIT;
  imgCi.flags = 0;
  vkCreateImage(mDevice, &imgCi, nullptr, &mImage);
}

void Image::CreateDeviceMemory() {
  assert(mImage != VK_NULL_HANDLE && "VkImage needs to be created prior to creating VkDeviceMemory");
  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(mDevice, mImage,&reqs);
  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = reqs.size;
  allocInfo.memoryTypeIndex = FindMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); 
  vkAllocateMemory(mDevice, &allocInfo, nullptr, &mDeviceMemory);
}

void Image::CreateImageView() {
  assert(mImage != VK_NULL_HANDLE && "VkImage needs to be created prior to creating VkImageView");
  VkImageViewCreateInfo viewCi{}; 
  viewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCi.flags = 0;
  viewCi.image = mImage;
  viewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCi.format = mFormat;
  viewCi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewCi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewCi.subresourceRange.baseMipLevel = 0;
  viewCi.subresourceRange.levelCount = 1;
  viewCi.subresourceRange.baseArrayLayer = 0;
  viewCi.subresourceRange.layerCount = 1;
  vkCreateImageView(mDevice, &viewCi, nullptr, &mImageView);
}

void Image::CreateSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.flags = 0;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST; 
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  vkCreateSampler(mDevice, &samplerInfo, nullptr, &mSampler);
}

void Image::CreateStagingBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size) {
  // Create Buffer
  VkBufferCreateInfo bufferCi{};
  bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCi.size = size;
  bufferCi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(mDevice, &bufferCi, nullptr, &buffer);
  // Allocate Memory
  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(mDevice, buffer, &reqs); 
  VkMemoryAllocateInfo bufferMemAllocInfo{};
  bufferMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  bufferMemAllocInfo.allocationSize = reqs.size;
  bufferMemAllocInfo.memoryTypeIndex = FindMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vkAllocateMemory(mDevice, &bufferMemAllocInfo, nullptr, &memory);
  // Bind
  vkBindBufferMemory(mDevice, buffer, memory, 0);
}

void Image::Load(VkQueue queue, VkCommandBuffer commandBuffer, const uint8_t* const imgData) {
  assert(mImage != VK_NULL_HANDLE && "VkImage must be created before we can transfer to it");
  assert(mDeviceMemory != VK_NULL_HANDLE && "VkImage must be created before we can transfer to it");
  if (mIsLoaded) {
    return;
  }
  
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;

  const size_t imgSize = mInfo.width * mInfo.height * mInfo.bytesPerPixel;
  CreateStagingBuffer(stagingBuffer, stagingMemory, imgSize);
  
  // Map and Write to Staging Memory
  void* memRegion;
  vkMapMemory(mDevice, stagingMemory, 0, VK_WHOLE_SIZE, 0, &memRegion);
  std::memcpy(memRegion, imgData, imgSize);
  vkUnmapMemory(mDevice, stagingMemory);
  
  // Use Command buffer and Queue to Transfer it over.
  // and Transition Image Layout
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);
  
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = mImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0; 
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(mInfo.width), static_cast<uint32_t>(mInfo.height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    VkImageMemoryBarrier barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.image = mImage;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseMipLevel = 0;
    barrier2.subresourceRange.levelCount = 1;
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 1;
    barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo subInfo{};
    subInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    subInfo.waitSemaphoreCount = 0;
    subInfo.commandBufferCount = 1; 
    subInfo.pCommandBuffers = &commandBuffer;
    subInfo.signalSemaphoreCount = 0;
    vkQueueSubmit(queue, 1, &subInfo, VK_NULL_HANDLE);
    vkDeviceWaitIdle(mDevice);

    vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
    vkFreeMemory(mDevice, stagingMemory, nullptr);

    mIsLoaded = true;
    mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void Image::Load(VkQueue queue, VkCommandBuffer commandBuffer) {
  assert(mImage != VK_NULL_HANDLE && "VkImage must be created before we can transfer to it");
  assert(mDeviceMemory != VK_NULL_HANDLE && "VkImage must be created before we can transfer to it");
  if (mIsLoaded) {
    return;
  }

  int x, y, comp;
  uint8_t* imgData = stbi_load(mFile.c_str(), &x, &y, &comp, mInfo.bytesPerPixel);
  if (!imgData) {
    throw std::runtime_error("Failed to load" + mFile);
  }

  Load(queue, commandBuffer, imgData);  
  stbi_image_free(imgData);
}

