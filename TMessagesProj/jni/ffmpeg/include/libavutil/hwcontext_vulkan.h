#ifndef AVUTIL_HWCONTEXT_VULKAN_H
#define AVUTIL_HWCONTEXT_VULKAN_H

#if defined(_WIN32) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include "pixfmt.h"
#include "frame.h"
#include "hwcontext.h"

typedef struct AVVkFrame AVVkFrame;

typedef struct AVVulkanDeviceQueueFamily {

    int idx;

    int num;

    VkQueueFlagBits flags;

    VkVideoCodecOperationFlagBitsKHR video_caps;
} AVVulkanDeviceQueueFamily;

typedef struct AVVulkanDeviceContext {

    const VkAllocationCallbacks *alloc;

    PFN_vkGetInstanceProcAddr get_proc_addr;

    VkInstance inst;

    VkPhysicalDevice phys_dev;

    VkDevice act_dev;

    VkPhysicalDeviceFeatures2 device_features;

    const char * const *enabled_inst_extensions;
    int nb_enabled_inst_extensions;

    const char * const *enabled_dev_extensions;
    int nb_enabled_dev_extensions;

#if FF_API_VULKAN_FIXED_QUEUES

    attribute_deprecated
    int queue_family_index;
    attribute_deprecated
    int nb_graphics_queues;

    attribute_deprecated
    int queue_family_tx_index;
    attribute_deprecated
    int nb_tx_queues;

    attribute_deprecated
    int queue_family_comp_index;
    attribute_deprecated
    int nb_comp_queues;

    attribute_deprecated
    int queue_family_encode_index;
    attribute_deprecated
    int nb_encode_queues;

    attribute_deprecated
    int queue_family_decode_index;
    attribute_deprecated
    int nb_decode_queues;
#endif

#if FF_API_VULKAN_SYNC_QUEUES

    attribute_deprecated
    void (*lock_queue)(struct AVHWDeviceContext *ctx, uint32_t queue_family, uint32_t index);

    attribute_deprecated
    void (*unlock_queue)(struct AVHWDeviceContext *ctx, uint32_t queue_family, uint32_t index);
#endif

    AVVulkanDeviceQueueFamily qf[64];
    int nb_qf;
} AVVulkanDeviceContext;

typedef enum AVVkFrameFlags {

    AV_VK_FRAME_FLAG_NONE              = (1ULL << 0),

    AV_VK_FRAME_FLAG_DISABLE_MULTIPLANE = (1ULL << 2),
} AVVkFrameFlags;

typedef struct AVVulkanFramesContext {

    VkImageTiling tiling;

    VkImageUsageFlagBits usage;

    void *create_pnext;

    void *alloc_pnext[AV_NUM_DATA_POINTERS];

    AVVkFrameFlags flags;

    VkImageCreateFlags img_flags;

    VkFormat format[AV_NUM_DATA_POINTERS];

    int nb_layers;

    void (*lock_frame)(struct AVHWFramesContext *fc, AVVkFrame *vkf);

    void (*unlock_frame)(struct AVHWFramesContext *fc, AVVkFrame *vkf);
} AVVulkanFramesContext;

struct AVVkFrame {

    VkImage img[AV_NUM_DATA_POINTERS];

    VkImageTiling tiling;

    VkDeviceMemory mem[AV_NUM_DATA_POINTERS];
    size_t size[AV_NUM_DATA_POINTERS];

    VkMemoryPropertyFlagBits flags;

    VkAccessFlagBits access[AV_NUM_DATA_POINTERS];
    VkImageLayout layout[AV_NUM_DATA_POINTERS];

    VkSemaphore sem[AV_NUM_DATA_POINTERS];

    uint64_t sem_value[AV_NUM_DATA_POINTERS];

    struct AVVkFrameInternal *internal;

    ptrdiff_t offset[AV_NUM_DATA_POINTERS];

    uint32_t queue_family[AV_NUM_DATA_POINTERS];
};

AVVkFrame *av_vk_frame_alloc(void);

const VkFormat *av_vkfmt_from_pixfmt(enum AVPixelFormat p);

const char **av_vk_get_optional_instance_extensions(int *count);

const char **av_vk_get_optional_device_extensions(int *count);

#endif
