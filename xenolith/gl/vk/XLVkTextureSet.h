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

	uint32_t getImageCount() const { return _imageCount; }
	VkDescriptorSetLayout getLayout() const { return _layout; }
	const Rc<ImageView> &getDefaultImageView() const { return _defaultImageView; }

	Rc<TextureSet> acquireSet(Device &dev);
	void releaseSet(Rc<TextureSet> &&);

	bool isDefaultInit() const { return _defaultInit; }
	VkImageMemoryBarrier writeDefaults(Device &dev, VkCommandBuffer);

protected:
	uint32_t _imageCount = 0;
	VkDescriptorSetLayout _layout = VK_NULL_HANDLE;

	bool _defaultInit = false;
	Rc<Image> _defaultImage;
	Rc<ImageView> _defaultImageView;

	mutable Mutex _mutex;
	Vector<Rc<TextureSet>> _sets;
};

class TextureSet : public gl::TextureSet {
public:
	virtual ~TextureSet() { }

	bool init(Device &dev, const TextureSetLayout &);

	VkDescriptorSet getSet() const { return _set; }

	virtual void write(const gl::MaterialLayout &) override;

protected:
	const TextureSetLayout *_layout = nullptr;
	uint32_t _count = 0;
	VkDescriptorSet _set = VK_NULL_HANDLE;
	VkDescriptorPool _pool = VK_NULL_HANDLE;
};

}

#endif /* XENOLITH_GL_VK_XLVKTEXTURESET_H_ */
