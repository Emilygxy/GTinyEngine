#pragma once
#include "VK_Base.h"
using namespace vk;

extern const VkExtent2D& windowSize;

namespace easy_vk {

    struct renderPassWithFramebuffers {
        RenderPass renderPass;
        std::vector<Framebuffer> framebuffers;
    };

    const renderPassWithFramebuffers& CreateRpwf_Screen();

    class stagingBuffer {
        static inline class {
            stagingBuffer* pointer = Create();
            stagingBuffer* Create() 
            {
                static stagingBuffer stagingBuffer;
                pointer = &stagingBuffer;
                GraphicsBase::Base().AddCallback_DestroyDevice([] { stagingBuffer.~stagingBuffer(); });

                return pointer;
            }
            public:
            stagingBuffer& Get() const { return *pointer; }
        } stagingBuffer_mainThread;
    protected:
        bufferMemory bufferMemory;
        VkDeviceSize memoryUsage = 0;
        vk::image aliasedImage;
    public:
        stagingBuffer() = default;
        stagingBuffer(VkDeviceSize size) {
            Expand(size);
        }
        //Getter
        operator VkBuffer() const { return bufferMemory.Buffer(); }
        const VkBuffer* Address() const { return bufferMemory.AddressOfBuffer(); }
        VkDeviceSize AllocationSize() const { return bufferMemory.AllocationSize(); }
        VkImage AliasedImage() const { return aliasedImage; }
        //Const Function
        void RetrieveData(void* pData_src, VkDeviceSize size) const {
            bufferMemory.RetrieveData(pData_src, size);
        }
        //Non-const Function
        void Expand(VkDeviceSize size) {
            if (size <= AllocationSize())
                return;
            Release();
            VkBufferCreateInfo bufferCreateInfo = {
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            };
            bufferMemory.Create(bufferCreateInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }
        void Release() {
            bufferMemory.~bufferMemory();
        }
        void* MapMemory(VkDeviceSize size) {
            Expand(size);
            void* pData_dst = nullptr;
            bufferMemory.MapMemory(pData_dst, size);
            memoryUsage = size;
            return pData_dst;
        }
        void UnmapMemory() {
            bufferMemory.UnmapMemory(memoryUsage);
            memoryUsage = 0;
        }
        void BufferData(const void* pData_src, VkDeviceSize size) {
            Expand(size);
            bufferMemory.BufferData(pData_src, size);
        }
        [[nodiscard]]
        VkImage AliasedImage2d(VkFormat format, VkExtent2D extent) {
            if (!(FormatProperties(format).linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
                return VK_NULL_HANDLE;
            VkDeviceSize imageDataSize = VkDeviceSize(FormatInfo(format).sizePerPixel) * extent.width * extent. height;
            if (imageDataSize > AllocationSize())
                return VK_NULL_HANDLE;
            VkImageFormatProperties imageFormatProperties = {};
            vkGetPhysicalDeviceImageFormatProperties(GraphicsBase::Base().PhysicalDevice(),
                format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &imageFormatProperties);
            if (extent.width > imageFormatProperties.maxExtent.width ||
                extent.height > imageFormatProperties.maxExtent.height ||
                imageDataSize > imageFormatProperties.maxResourceSize)
                return VK_NULL_HANDLE;
            VkImageCreateInfo imageCreateInfo = {
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = { extent.width, extent.height, 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
            };
            aliasedImage.~image();
            aliasedImage.Create(imageCreateInfo);
            VkImageSubresource subResource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
            VkSubresourceLayout subresourceLayout = {};
            vkGetImageSubresourceLayout(GraphicsBase::Base().Device(), aliasedImage, &subResource, &subresourceLayout);
            if (subresourceLayout.size != imageDataSize)
                return VK_NULL_HANDLE;
            aliasedImage.BindMemory(bufferMemory.Memory());
            return aliasedImage;
        }
        //Static Function
        static VkBuffer Buffer_MainThread() {
            return stagingBuffer_mainThread.Get();
        }
        static void Expand_MainThread(VkDeviceSize size) {
            stagingBuffer_mainThread.Get().Expand(size);
        }
        static void Release_MainThread() {
            stagingBuffer_mainThread.Get().Release();
        }
        static void* MapMemory_MainThread(VkDeviceSize size) {
            return stagingBuffer_mainThread.Get().MapMemory(size);
        }
        static void UnmapMemory_MainThread() {
            stagingBuffer_mainThread.Get().UnmapMemory();
        }
        static void BufferData_MainThread(const void* pData_src, VkDeviceSize size) {
            stagingBuffer_mainThread.Get().BufferData(pData_src, size);
        }
        static void RetrieveData_MainThread(void* pData_src, VkDeviceSize size) {
            stagingBuffer_mainThread.Get().RetrieveData(pData_src, size);
        }
        [[nodiscard]]
        static VkImage AliasedImage2d_MainThread(VkFormat format, VkExtent2D extent) {
            return stagingBuffer_mainThread.Get().AliasedImage2d(format, extent);
        }
    };

    class deviceLocalBuffer {
        protected:
            bufferMemory bufferMemory;
        public:
            deviceLocalBuffer() = default;
            deviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst) {
                Create(size, desiredUsages_Without_transfer_dst);
            }
            //Getter
            operator VkBuffer() const { return bufferMemory.Buffer(); }
            const VkBuffer* Address() const { return bufferMemory.AddressOfBuffer(); }
            VkDeviceSize AllocationSize() const { return bufferMemory.AllocationSize(); }
            //Non-const Function
            void Create(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst);
            void Recreate(VkDeviceSize size, VkBufferUsageFlags desiredUsages_Without_transfer_dst) 
            {
                GraphicsBase::Base().WaitIdle(); // The deviceLocalBuffer encapsulated buffer may be frequently used in each frame, and it should be ensured that the physical device is not using it before rebuilding it
                bufferMemory.~bufferMemory();
                Create(size, desiredUsages_Without_transfer_dst);
            }

            //Const Function
            // Suitable for updating continuous data blocks
            void TransferData(const void* pData_src, VkDeviceSize size, VkDeviceSize offset = 0) const;
            // Suitable for updating multiple discontinuous data blocks, stride is the step length between each group of data, here offset is obviously the offset in the target buffer
            void TransferData(const void* pData_src, uint32_t elementCount, VkDeviceSize elementSize, VkDeviceSize stride_src, VkDeviceSize stride_dst, VkDeviceSize offset = 0) const;
            // Suitable for updating continuous data blocks from the beginning of the buffer, the data size is automatically determined
            void TransferData(const auto& data_src) const {
                TransferData(&data_src, sizeof data_src);
            }
        };

        class vertexBuffer :public deviceLocalBuffer {
            public:
                vertexBuffer() = default;
                vertexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) :deviceLocalBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages) {}
                //Non-const Function
                void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
                    deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
                }
                void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
                    deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | otherUsages);
                }
            };
        class indexBuffer :public deviceLocalBuffer {
        public:
            indexBuffer() = default;
            indexBuffer(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) :deviceLocalBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages) {}
            //Non-const Function
            void Create(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
                deviceLocalBuffer::Create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
            }
            void Recreate(VkDeviceSize size, VkBufferUsageFlags otherUsages = 0) {
                deviceLocalBuffer::Recreate(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | otherUsages);
            }
        };
}