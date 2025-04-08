#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

using ImageId_t = int;

struct ImageInfo {
  int width;
  int height;
  int bytesPerPixel;
};

class Image {
 public:
   Image();
   Image(VkPhysicalDevice physicalDevice, VkDevice device, const std::string& imgFile);
   Image(VkPhysicalDevice physicalDevice, VkDevice device, 
         const int width, const int height, const int bytesPerPixel);

   ~Image();
  
   Image& operator=(const Image& image) = delete;
   Image& operator=(Image&& image);
   
   void Load(VkQueue queue, VkCommandBuffer buffer);
   void Load(VkQueue queue, VkCommandBuffer buffer, const uint8_t* data);
   void CreateDescriptorSets(VkDescriptorSetLayout layout);

   bool isLoaded() const {
     return mIsLoaded;
   }
  
   ImageInfo GetImageInfo() const {
     return mInfo;
   }

   VkImage GetImage() const {
     return mImage;
   }

   VkImageLayout GetImageLayout() const {
     return mImageLayout;
   }

   VkImageView GetImageView() const {
     return mImageView;
   }

   VkSampler GetSampler() const {
     return mSampler; 
   }

 private:
  void CreateImage();
  void CreateDeviceMemory();
  void CreateImageView();
  void CreateSampler();
  void CreateStagingBuffer(VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize);
  uint32_t FindMemoryType(uint32_t allowableMemoryTypes, 
                          VkMemoryPropertyFlags requestedProperties) const;

  ImageInfo mInfo;
  std::string mFile;
  VkPhysicalDevice mPhysicalDevice;
  VkDevice mDevice;
  VkImage mImage;
  VkImageView mImageView;
  VkFormat mFormat;
  VkDeviceMemory mDeviceMemory;
  VkSampler mSampler;
  VkImageLayout mImageLayout;
  bool mIsLoaded;
};
