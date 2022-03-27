/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_VK_XLVKTEXTURESET_H_
#define XENOLITH_GL_VK_XLVKTEXTURESET_H_

#include "XLVkDevice.h"
#include "XLVkObject.h"

namespace stappler::xenolith::vk {

class TextureSet;

// persistent object, part of Device
class TextureSetLayout : public Ref {
public:
	virtual ~TextureSetLayout() { }

	bool init(Device &dev, uint32_t);
	void invalidate(Device &dev);

	bool compile(Device &dev, const Vector<VkSampler> &);

	const uint32_t &getImageCount() const { return _imageCount; }
	VkDescriptorSetLayout getLayout() const { return _layout; }
	const Rc<ImageView> &getEmptyImageView() const { return _emptyImageView; }
	const Rc<ImageView> &getSolidImageView() const { return _solidImageView; }

	Rc<TextureSet> acquireSet(Device &dev);
	void releaseSet(Rc<TextureSet> &&);

	void initDefault(Device &dev, gl::Loop &);

	bool isPartiallyBound() const { return _partiallyBound; }

	Rc<Image> getEmptyImageObject() const;
	Rc<Image> getSolidImageObject() const;

	void compileImage(Device &dev, gl::Loop &loop, const Rc<gl::DynamicImage> &, Function<void(bool)> &&);

protected:
	void writeDefaults(Device &dev, VkCommandBuffer buf);
	void writeImageTransfer(Device &dev, VkCommandBuffer buf, uint32_t qidx, const Rc<Buffer> &, const Rc<Image> &);

	bool _partiallyBound = false;
	uint32_t _imageCount = 0;
	VkDescriptorSetLayout _layout = VK_NULL_HANDLE;

	Rc<Image> _emptyImage;
	Rc<ImageView> _emptyImageView;
	Rc<Image> _solidImage;
	Rc<ImageView> _solidImageView;

	mutable Mutex _mutex;
	Vector<Rc<TextureSet>> _sets;
};

class TextureSet : public gl::TextureSet {
public:
	virtual ~TextureSet() { }

	bool init(Device &dev, const TextureSetLayout &);

	VkDescriptorSet getSet() const { return _set; }

	virtual void write(const gl::MaterialLayout &) override;

	const Vector<VkImageMemoryBarrier> &getPendingBarriers() const { return _pendingBarriers; }
	void dropPendingBarriers();

	Device *getDevice() const;

protected:
	bool _partiallyBound = false;
	const TextureSetLayout *_layout = nullptr;
	uint32_t _count = 0;
	VkDescriptorSet _set = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
	Vector<VkImageMemoryBarrier> _pendingBarriers;
};

}

#endif /* XENOLITH_GL_VK_XLVKTEXTURESET_H_ */
