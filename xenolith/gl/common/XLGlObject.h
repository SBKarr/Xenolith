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

#ifndef XENOLITH_GL_COMMON_XLGLOBJECT_H_
#define XENOLITH_GL_COMMON_XLGLOBJECT_H_

#include "XLGl.h"

namespace stappler::xenolith::gl {

class ObjectInterface {
public:
	using ClearCallback = void (*) (Device *, ObjectType, void *);

	virtual ~ObjectInterface() { }
	virtual bool init(Device &, ClearCallback, ObjectType, void *ptr);
	virtual void invalidate();

	ObjectType getType() const { return _type; }

protected:
	ObjectType _type;
	Device *_device = nullptr;
	ClearCallback _callback = nullptr;
	void *_ptr = nullptr;
};


class NamedObject : public NamedRef, public ObjectInterface {
public:
	virtual ~NamedObject();
};


class Object : public Ref, public ObjectInterface {
public:
	virtual ~Object();
};


class Pipeline : public NamedObject {
public:
	virtual ~Pipeline() { }

	virtual StringView getName() const override { return _name; }

protected:
	String _name;
};


class Shader : public NamedObject {
public:
	virtual ~Shader() { }

	virtual StringView getName() const override { return _name; }
	virtual ProgramStage getStage() const { return _stage; }

protected:
	String _name;
	ProgramStage _stage = ProgramStage::None;
};


class RenderPassImpl : public NamedObject {
public:
	virtual ~RenderPassImpl() { }

	virtual StringView getName() const override { return _name; }

protected:
	String _name;
};

class Framebuffer : public Object {
public:
	virtual ~Framebuffer() { }

protected:
	Rc<RenderPassImpl> _renderPass;
	Vector<Rc<ImageView>> imageViews;
};

class Image : public Object {
public:
	virtual ~Image() { }

	const ImageInfo &getInfo() const { return _info; }

protected:
	ImageInfo _info;
};

class ImageView : public Object {
public:
	virtual ~ImageView() { }

	const Rc<Image> &getImage() const { return _image; }

protected:
	Rc<Image> _image;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLOBJECT_H_ */
