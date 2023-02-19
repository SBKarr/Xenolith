/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/


#ifndef XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_
#define XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_

#include "XLVkInstance.h"
#include "XLGlDevice.h"
#include "XLGlLoop.h"
#include "XLRenderQueueFrameHandle.h"

namespace stappler::xenolith::vk {

class Device;
class DeviceQueue;
class CommandPool;
class Semaphore;
class Fence;
class Loop;
class Image;
class ImageView;
class Buffer;
class RenderPassImpl;
class Framebuffer;
class GraphicPipeline;
class ComputePipeline;
class CommandBuffer;
class DescriptorSet;
class DeviceMemoryPool;

using PipelineDescriptor = renderqueue::PipelineDescriptor;

struct DeviceQueueFamily {
	using FrameHandle = renderqueue::FrameHandle;

	struct Waiter {
		Function<void(Loop &, const Rc<DeviceQueue> &)> acquireForLoop;
		Function<void(Loop &)> releaseForLoop;
		Function<void(FrameHandle &, const Rc<DeviceQueue> &)> acquireForFrame;
		Function<void(FrameHandle &)> releaseForFrame;

		Rc<FrameHandle> handle;
		Rc<Loop> loop;
		Rc<Ref> ref;

		Waiter(Function<void(FrameHandle &, const Rc<DeviceQueue> &)> &&a, Function<void(FrameHandle &)> &&r,
				FrameHandle *h, Rc<Ref> &&ref)
		: acquireForFrame(move(a)), releaseForFrame(move(r)), handle(h), ref(move(ref)) { }

		Waiter(Function<void(Loop &, const Rc<DeviceQueue> &)> &&a, Function<void(Loop &)> &&r,
				Loop *h, Rc<Ref> &&ref)
		: acquireForLoop(move(a)), releaseForLoop(move(r)), loop(h), ref(move(ref)) { }
	};

	uint32_t index;
	uint32_t count;
	QueueOperations preferred = QueueOperations::None;
	QueueOperations ops = QueueOperations::None;
	VkExtent3D transferGranularity;
	Vector<Rc<DeviceQueue>> queues;
	Vector<Rc<CommandPool>> pools;
	Vector<Waiter> waiters;
};

class DeviceQueue final : public Ref {
public:
	using FrameSync = renderqueue::FrameSync;
	using FrameHandle = renderqueue::FrameHandle;

	virtual ~DeviceQueue();

	virtual bool init(Device &, VkQueue, uint32_t, QueueOperations);

	bool submit(const FrameSync &, Fence &, CommandPool &, SpanView<const CommandBuffer *>);
	bool submit(Fence &, const CommandBuffer *);
	bool submit(Fence &, SpanView<const CommandBuffer *>);

	void waitIdle();

	uint32_t getActiveFencesCount();
	void retainFence(const Fence &);
	void releaseFence(const Fence &);

	uint32_t getIndex() const { return _index; }
	uint64_t getFrameIndex() const { return _frameIdx; }
	VkQueue getQueue() const { return _queue; }
	QueueOperations getOps() const { return _ops; }
	VkResult getResult() const { return _result; }

	void setOwner(FrameHandle &);
	void reset();

protected:
	Device *_device = nullptr;
	uint32_t _index = 0;
	uint64_t _frameIdx = 0;
	QueueOperations _ops = QueueOperations::None;
	VkQueue _queue;
	std::atomic<uint32_t> _nfences;
	VkResult _result = VK_ERROR_UNKNOWN;
};

enum class BufferLevel {
	Primary = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	Secondary = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
};

struct QueueFamilyTransfer {
	uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

struct ImageMemoryBarrier {
	ImageMemoryBarrier() = default;

	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, VkImageSubresourceRange);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer);
	ImageMemoryBarrier(Image *, VkAccessFlags src, VkAccessFlags dst,
			VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer, VkImageSubresourceRange);
	ImageMemoryBarrier(const VkImageMemoryBarrier &);

	VkAccessFlags srcAccessMask = 0;
	VkAccessFlags dstAccessMask = 0;
	VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	QueueFamilyTransfer familyTransfer;
	Image *image = nullptr;
	VkImageSubresourceRange subresourceRange;
};

struct BufferMemoryBarrier {
	BufferMemoryBarrier() = default;
	BufferMemoryBarrier(Buffer *, VkAccessFlags src, VkAccessFlags dst);
	BufferMemoryBarrier(Buffer *, VkAccessFlags src, VkAccessFlags dst,
			QueueFamilyTransfer, VkDeviceSize, VkDeviceSize);
	BufferMemoryBarrier(const VkBufferMemoryBarrier &);

	VkAccessFlags srcAccessMask = 0;
	VkAccessFlags dstAccessMask = 0;
	QueueFamilyTransfer familyTransfer;
	Buffer *buffer = nullptr;
	VkDeviceSize offset = 0;
	VkDeviceSize size = VK_WHOLE_SIZE;
};

struct DescriptorInfo {
	DescriptorInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
	: descriptor(desc), index(index), external(external) { }

	const PipelineDescriptor *descriptor;
	uint32_t index;
	bool external;
};

struct DescriptorImageInfo : public DescriptorInfo {
	DescriptorImageInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
	: DescriptorInfo(desc, index, external) { }

	Rc<ImageView> imageView;
	VkSampler sampler = VK_NULL_HANDLE;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct DescriptorBufferInfo : public DescriptorInfo {
	DescriptorBufferInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
	: DescriptorInfo(desc, index, external) { }

	Rc<Buffer> buffer;
	VkDeviceSize offset = 0;
	VkDeviceSize range = VK_WHOLE_SIZE;
};

struct DescriptorBufferViewInfo : public DescriptorInfo {
	DescriptorBufferViewInfo(const PipelineDescriptor *desc, uint32_t index, bool external)
	: DescriptorInfo(desc, index, external) { }

	Rc<Buffer> buffer;
	VkBufferView target = VK_NULL_HANDLE;
};

class CommandBuffer : public Ref {
public:
	virtual ~CommandBuffer();

	bool init(const CommandPool *, const DeviceTable *, VkCommandBuffer);
	void invalidate();

	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<ImageMemoryBarrier>);
	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>);
	void cmdPipelineBarrier(VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>, SpanView<ImageMemoryBarrier>);

	void cmdCopyBuffer(Buffer *src, Buffer *dst);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy>);

	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, VkFilter filter = VK_FILTER_LINEAR);
	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, const VkImageCopy &copy);
	void cmdCopyImage(Image *src, VkImageLayout, Image *dst, VkImageLayout, SpanView<VkImageCopy>);

	void cmdCopyBufferToImage(Buffer *, Image *, VkImageLayout, VkDeviceSize offset);
	void cmdCopyBufferToImage(Buffer *, Image *, VkImageLayout, SpanView<VkBufferImageCopy>);

	void cmdCopyImageToBuffer(Image *, VkImageLayout, Buffer *buf, VkDeviceSize offset);
	void cmdCopyImageToBuffer(Image *, VkImageLayout, Buffer *buf, SpanView<VkBufferImageCopy>);

	void cmdClearColorImage(Image *, VkImageLayout, const Color4F &);

	void cmdBeginRenderPass(RenderPassImpl *pass, Framebuffer *fb, VkSubpassContents subpass, bool alt = false);
	void cmdEndRenderPass();

	void cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports);
	void cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors);

	void cmdBindPipeline(GraphicPipeline *);
	void cmdBindPipeline(ComputePipeline *);

	void cmdBindIndexBuffer(Buffer *, VkDeviceSize offset, VkIndexType indexType);

	void cmdBindDescriptorSets(RenderPassImpl *, uint32_t firstSet = 0);
	void cmdBindDescriptorSets(RenderPassImpl *, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);

	void cmdBindGraphicDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);
	void cmdBindComputeDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);

	void cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	void cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
			int32_t vertexOffset, uint32_t firstInstance);

	void cmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, BytesView);

	void cmdFillBuffer(Buffer *, uint32_t data);
	void cmdFillBuffer(Buffer *, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data);

	void cmdDispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

	void cmdNextSubpass();

	VkCommandBuffer getBuffer() const { return _buffer; }

protected:
	void addImage(Image *);
	void addBuffer(Buffer *);

	const CommandPool *_pool = nullptr;
	const DeviceTable *_table = nullptr;
	VkCommandBuffer _buffer = VK_NULL_HANDLE;

	Set<Rc<Buffer>> _buffers;
	Set<Rc<Image>> _images;
	Set<Rc<Framebuffer>> _framebuffers;
	Set<Rc<DescriptorSet>> _descriptorSets;
	Set<Rc<DeviceMemoryPool>> _memPool;
};

class CommandPool : public Ref {
public:
	static constexpr VkCommandBufferUsageFlagBits DefaultFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	using Level = BufferLevel;

	virtual ~CommandPool();

	bool init(Device &dev, uint32_t familyIdx, QueueOperations c = QueueOperations::Graphics, bool transient = true);
	void invalidate(Device &dev);

	QueueOperations getClass() const { return _class; }
	uint32_t getFamilyIdx() const { return _familyIdx; }
	VkCommandPool getCommandPool() const { return _commandPool; }

	const CommandBuffer * recordBuffer(Device &dev, const Callback<bool(CommandBuffer &)> &,
			VkCommandBufferUsageFlagBits = DefaultFlags, Level = Level::Primary);

	void freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &);
	void reset(Device &dev, bool release = false);

	void autorelease(Rc<Ref> &&);

protected:
	uint32_t _familyIdx = 0;
	uint32_t _currentComplexity = 0;
	uint32_t _bestComplexity = 0;
	QueueOperations _class = QueueOperations::Graphics;
	VkCommandPool _commandPool = VK_NULL_HANDLE;
	Vector<Rc<Ref>> _autorelease;
	Vector<Rc<CommandBuffer>> _buffers;
};

}

#endif /* XENOLITH_GL_VK_XLVKDEVICEQUEUE_H_ */
