//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		vulkan wrapper
//
// $NoKeywords: $vk
//===============================================================================//

#ifndef VULKANINTERFACE_H
#define VULKANINTERFACE_H

#include "cbase.h"

#ifdef MCENGINE_FEATURE_VULKAN

#ifdef _WIN32

#define VK_USE_PLATFORM_WIN32_KHR

#else

// #error "TODO: add correct define here"

#endif

#ifdef MCENGINE_USE_SYSTEM_VULKAN
#include <vulkan/vulkan.h>
#else
#include <vulkan.h>
#endif

#endif

class ConVar;

class VulkanInterface {
   public:
    static ConVar *debug_vulkan;

   public:
    VulkanInterface();
    ~VulkanInterface();

    void finish();

    inline u32 getQueueFamilyIndex() const { return m_iQueueFamilyIndex; }
    inline bool isReady() const { return m_bReady; }

#ifdef MCENGINE_FEATURE_VULKAN

    // ILLEGAL:
    inline VkInstance getInstance() const { return m_instance; }
    inline VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    inline VkDevice getDevice() const { return m_device; }

#endif

   private:
    bool m_bReady;
    u32 m_iQueueFamilyIndex;

#ifdef MCENGINE_FEATURE_VULKAN

    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkCommandPool m_commandPool;

#endif
};

extern VulkanInterface *vulkan;

#endif
