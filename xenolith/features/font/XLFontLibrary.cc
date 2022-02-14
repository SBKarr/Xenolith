/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLFontLibrary.h"
#include "XLFontFace.h"
#include "XLApplication.h"
#include "XLGlRenderQueue.h"
#include "XLGlLoop.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace stappler::xenolith::font {

FontController::~FontController() {
	// image need to be finalized to remove cycled refs
	_image->finalize();
}

bool FontController::init(const Rc<FontLibrary> &lib, Rc<gl::DynamicImage> &&image) {
	_library = lib;
	_image = move(image);
	_texture = Rc<Texture>::create(_image);
	return true;
}

void FontController::updateTexture(Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&vec) {
	_library->updateImage(_image, move(vec));
}

FontLibrary::FontLibrary() {
	FT_Init_FreeType( &_library );
}
FontLibrary::~FontLibrary() {
	if (_library) {
		FT_Done_FreeType(_library);
		_library = nullptr;
	}
}

bool FontLibrary::init(const Rc<gl::Loop> &loop, Rc<gl::RenderQueue> &&queue) {
	_loop = loop;
	_queue = move(queue);
	if (_queue->isCompiled()) {
		onActivated();
	} else {
		auto linkId = retain();
		_loop->compileRenderQueue(_queue, [this, linkId] (bool success) {
			if (success) {
				_loop->getApplication()->performOnMainThread([this] {
					onActivated();
				}, this);
			}
			release(linkId);
		});
	}
	return true;
}

Rc<FontFaceObject> FontLibrary::openFontFace(SystemFontName name, FontSize size) {
	return openFontFace(getSystemFontName(name), size, [name] () -> FontData {
		// decompress file from app bundle
		auto d = getSystemFont(name);
		auto fontData = data::decompress<memory::StandartInterface>(d.data(), d.size());

		return FontData(move(fontData));
	});
}

Rc<FontFaceObject> FontLibrary::openFontFace(StringView dataName, FontSize size, const Callback<FontData()> &dataCallback) {
	auto faceName = toString(dataName, "?size=", size.get());

	std::unique_lock<Mutex> lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			return it->second;
		}
	} while (0);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		auto face = newFontFace(it->second->getView());
		auto ret = Rc<FontFaceObject>::create(it->second, face, size, _nextId);
		++ _nextId;
		if (ret) {
			_faces.emplace(faceName, ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	auto fontData = dataCallback();
	Rc<FontFaceData> dataObject;
	if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(move(fontData.bytes));
	}
	if (dataObject) {
		_data.emplace(dataName.str(), dataObject);
		auto face = newFontFace(dataObject->getView());
		auto ret = Rc<FontFaceObject>::create(it->second, face, size, _nextId);
		++ _nextId;
		if (ret) {
			_faces.emplace(faceName, ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	return nullptr;
}

void FontLibrary::update() {
	std::unique_lock<Mutex> lock(_mutex);

	do {
		auto it = _faces.begin();
		while (it != _faces.end()) {
			if (it->second->getReferenceCount() == 1) {
				doneFontFace(it->second->getFace());
				it = _faces.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _data.begin();
		while (it != _data.end()) {
			if (it->second->getReferenceCount() == 1) {
				it = _data.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);
}

void FontLibrary::acquireController(StringView key, Function<void(Rc<FontController> &&)> &&cb) {
	if (_active) {
		runAcquire(key, move(cb));
	} else {
		_pending.emplace_back(key.str(), move(cb));
	}
}

void FontLibrary::updateImage(const Rc<gl::DynamicImage> &image, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&data) {
	auto input = Rc<gl::RenderFontInput>::alloc();
	input->image = image;
	input->requests = move(data);
	input->output = [] (const gl::ImageInfo &info, BytesView data) {
		Bitmap bmp(data, info.extent.width, info.extent.height, Bitmap::PixelFormat::A8);
		bmp.save(Bitmap::FileFormat::Png, toString(Time::now().toMicros(), ".png"));
	};

	auto req = Rc<gl::FrameRequest>::create(_queue);

	for (auto &it : _queue->getInputAttachments()) {
		req->addInput(it, move(input));
		break;
	}

	_loop->runRenderQueue(move(req));
}

FT_Face FontLibrary::newFontFace(BytesView data) {
	FT_Face ret = nullptr;
	FT_Error err = FT_Err_Ok;

	err = FT_New_Memory_Face(_library, data.data(), data.size(), 0, &ret );
	if (err != FT_Err_Ok) {
		auto str = FT_Error_String(err);
		log::text("font::FontLibrary", str ? StringView(str) : "Unknown error");
		return nullptr;
	}

	return ret;
}

void FontLibrary::doneFontFace(FT_Face face) {
	if (face) {
		FT_Done_Face(face);
	}
}

void FontLibrary::onActivated() {
	_active = true;

	for (auto &it : _pending) {
		runAcquire(it.first, move(it.second));
	}
	_pending.clear();
}

void FontLibrary::runAcquire(StringView key, Function<void(Rc<FontController> &&)> &&cb) {
	auto image = new Rc<gl::DynamicImage>( Rc<gl::DynamicImage>::create([&] (gl::DynamicImage::Builder &builder) {
		builder.setImage(key,
			gl::ImageInfo(
					Extent2(2, 2),
					gl::ImageUsage::Sampled | gl::ImageUsage::TransferSrc,
					gl::RenderPassType::Graphics,
					gl::ImageFormat::R8_UNORM
			), [] (const gl::ImageData::DataCallback &cb) {
				Bytes bytes; bytes.reserve(4);
				bytes.emplace_back(0);
				bytes.emplace_back(255);
				bytes.emplace_back(255);
				bytes.emplace_back(0);
				cb(bytes);
			}, nullptr);
		return true;
	}));
	_loop->compileImage(*image, [this, cb = move(cb), image] (bool success) {
		_loop->getApplication()->performOnMainThread([this, cb, image, success] {
			if (success) {
				auto c = Rc<FontController>::create(this, move(*image));
				cb(move(c));
			} else {
				cb(nullptr);
			}
			delete image;
		}, this);
	});
}

}
