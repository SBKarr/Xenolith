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
#include "XLGlLoop.h"
#include "XLGlView.h"
#include "XLRenderQueueQueue.h"
#include "XLRenderQueueImageStorage.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H
#include FT_ADVANCES_H

namespace stappler::xenolith::font {

#include "XLDefaultFont-RobotoMono-Italic-VariableFont_wght.ttf.cc"
#include "XLDefaultFont-RobotoMono-VariableFont_wght.ttf.cc"
#include "XLDefaultFont-RobotoFlex-VariableFont.ttf.cc"

FontFaceObjectHandle::~FontFaceObjectHandle() {
	if (_onDestroy) {
		_onDestroy(this);
		_onDestroy = nullptr;
	}
	_library = nullptr;
	_face = nullptr;
}

bool FontFaceObjectHandle::init(const Rc<FontLibrary> &lib, Rc<FontFaceObject> &&obj, Function<void(const FontFaceObjectHandle *)> &&onDestroy) {
	_face = move(obj);
	_library = lib;
	_onDestroy = move(onDestroy);
	return true;
}

bool FontFaceObjectHandle::acquireTexture(char16_t theChar, const Callback<void(const CharTexture &)> &cb) {
	return _face->acquireTextureUnsafe(theChar, cb);
}

BytesView FontLibrary::getFont(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return BytesView(); break;
	case DefaultFontName::RobotoFlex_VariableFont:
		return BytesView((const uint8_t *)s_font_RobotoFlex_VariableFont, sizeof(s_font_RobotoFlex_VariableFont));
		break;
	case DefaultFontName::RobotoMono_VariableFont:
		return BytesView((const uint8_t *)s_font_RobotoMono_VariableFont, sizeof(s_font_RobotoMono_VariableFont));
		break;
	case DefaultFontName::RobotoMono_Italic_VariableFont:
		return BytesView((const uint8_t *)s_font_RobotoMono_Italic_VariableFont, sizeof(s_font_RobotoMono_Italic_VariableFont));
		break;
	}
	return BytesView();
}

StringView FontLibrary::getFontName(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return StringView(); break;
	case DefaultFontName::RobotoFlex_VariableFont: return StringView("RobotoFlex_VariableFont"); break;
	case DefaultFontName::RobotoMono_VariableFont: return StringView("RobotoMono_VariableFont"); break;
	case DefaultFontName::RobotoMono_Italic_VariableFont: return StringView("RobotoMono_Italic_VariableFont"); break;
	}
	return StringView();
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

bool FontLibrary::init(const Rc<gl::Loop> &loop) {
	_loop = loop;
	_queue = _loop->makeRenderFontQueue();
	_application = _loop->getApplication();
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

Rc<FontFaceData> FontLibrary::openFontData(StringView dataName, FontLayoutParameters params, const Callback<FontData()> &dataCallback) {
	std::unique_lock<Mutex> lock(_mutex);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		return it->second;
	}

	if (!dataCallback) {
		return nullptr;
	}

	lock.unlock();
	auto fontData = dataCallback();
	if (fontData.view.empty() && !fontData.callback) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.callback) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.callback));
	} else if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.bytes));
	}
	if (dataObject) {
		lock.lock();
		_data.emplace(dataObject->getName(), dataObject);

		auto face = newFontFace(dataObject->getView());
		lock.unlock();
		dataObject->inspectVariableFont(params, face);
		lock.lock();
		doneFontFace(face);
	}
	return dataObject;
}

Rc<FontFaceObject> FontLibrary::openFontFace(StringView dataName, const FontSpecializationVector &spec, const Callback<FontData()> &dataCallback) {
	String faceName = toString(dataName, spec.getSpecializationArgs());

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
		auto ret = Rc<FontFaceObject>::create(faceName, it->second, face, spec, getNextId());
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	if (!dataCallback) {
		return nullptr;
	}

	auto fontData = dataCallback();
	if (fontData.view.empty()) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.bytes));
	}

	if (dataObject) {
		_data.emplace(dataObject->getName(), dataObject);
		auto face = newFontFace(dataObject->getView());
		auto ret = Rc<FontFaceObject>::create(faceName, it->second, face, spec, getNextId());
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	return nullptr;
}

Rc<FontFaceObject> FontLibrary::openFontFace(const Rc<FontFaceData> &dataObject, const FontSpecializationVector &spec) {
	String faceName = toString(dataObject->getName(), spec.getSpecializationArgs());

	std::unique_lock<Mutex> lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			return it->second;
		}
	} while (0);

	auto face = newFontFace(dataObject->getView());
	auto ret = Rc<FontFaceObject>::create(faceName, dataObject, face, spec, getNextId());
	if (ret) {
		_faces.emplace(ret->getName(), ret);
	} else {
		doneFontFace(face);
	}
	return ret;
}

void FontLibrary::update(uint64_t clock) {
	Vector<Rc<FontFaceObject>> erased;
	std::unique_lock<Mutex> lock(_mutex);

	do {
		auto it = _faces.begin();
		while (it != _faces.end()) {
			if (it->second->getReferenceCount() == 1) {
				releaseId(it->second->getId());
				doneFontFace(it->second->getFace());
				erased.emplace_back(it->second.get());
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

	_mutex.unlock();

	std::unique_lock uniqueLock(_sharedMutex);
	for (auto &it : erased) {
		_threads.erase(it.get());
	}
}

static Bytes openResourceFont(FontLibrary::DefaultFontName name) {
	auto d = FontLibrary::getFont(name);
	return data::decompress<memory::StandartInterface>(d.data(), d.size());
}

static String getResourceFontName(FontLibrary::DefaultFontName name) {
	return toString("resource:", FontLibrary::getFontName(name));
}

static const FontController::FontSource * makeResourceFontQuery(FontController::Builder &builder, FontLibrary::DefaultFontName name,
		FontLayoutParameters params = FontLayoutParameters()) {
	return builder.addFontSource( getResourceFontName(name), [name] { return openResourceFont(name); }, params);
}

FontController::Builder FontLibrary::makeDefaultControllerBuilder(StringView key) {
	FontController::Builder ret(key);

	auto res_RobotoFlex = makeResourceFontQuery(ret, DefaultFontName::RobotoFlex_VariableFont);
	auto res_RobotoMono_Variable = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_VariableFont);
	auto res_RobotoMono_Italic_Variable = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_Italic_VariableFont, FontLayoutParameters{
		FontStyle::Italic, FontWeight::Normal, FontStretch::Normal});

	ret.addFontFaceQuery("sans", res_RobotoFlex);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Variable);
	ret.addFontFaceQuery("monospace", res_RobotoMono_Italic_Variable);

	ret.addAlias("default", "monospace");

	return ret;
}

Rc<FontController> FontLibrary::acquireController(FontController::Builder &&b) {
	Rc<FontController> ret = Rc<FontController>::create(this);

	struct ControllerBuilder : Ref {
		FontController::Builder builder;
		Rc<FontController> controller;
		Rc<gl::DynamicImage> dynamicImage;

		bool invalid = false;
		std::atomic<size_t> pendingData = 0;
		FontLibrary *library = nullptr;

		ControllerBuilder(FontController::Builder &&b)
		: builder(move(b)) { }

		void invalidate() {
			controller = nullptr;
		}

		void loadData() {
			if (invalid) {
				invalidate();
				return;
			}

			library->getApplication()->perform([this] (const thread::Task &) -> bool {
				for (auto &it : builder.getData()->familyQueries) {
					Vector<Rc<FontFaceData>> d; d.reserve(it.second.sources.size());
					for (auto &iit : it.second.sources) {
						d.emplace_back(move(iit->data));
					}

					controller->addFont(it.second.family, move(d));
					/*for (auto &iit : it.second.chars) {
						if (!iit.second.empty()) {
							auto lId = controller->getLayout(FontParameters{
								it.second.style, it.second.weight, it.second.stretch,
								font::FontVariant::Normal,
								font::ListStyleType::None,
								iit.first, it.second.family
							}, 1.0f);
							controller->addString(lId, iit.second);
						}
					}*/
				}
				for (auto &it : builder.getData()->familyQueries) {
					Vector<Rc<FontFaceData>> d; d.reserve(it.second.sources.size());
					for (auto &iit : it.second.sources) {
						d.emplace_back(move(iit->data));
					}
				}
				return true;
			}, [this] (const Task &, bool success) {
				if (success) {
					controller->setAliases(builder.getAliases());
					controller->setLoaded(true);
				}
				controller = nullptr;
			}, this);
		}

		void onDataLoaded(bool success) {
			auto v = pendingData.fetch_sub(1);
			if (!success) {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			} else if (v == 1) {
				loadData();
			}
		}

		void onImageLoaded(Rc<gl::DynamicImage> &&image) {
			auto v = pendingData.fetch_sub(1);
			if (image) {
				controller->setImage(move(image));
				if (v == 1) {
					loadData();
				}
			} else {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			}
		}
	};

	auto builder = Rc<ControllerBuilder>::alloc(move(b));
	builder->library = this;
	builder->controller = ret;

	builder->pendingData = builder->builder.getData()->dataQueries.size() + 1;

	for (auto &it : builder->builder.getData()->dataQueries) {
		_application->perform([this, name = it.first, sourcePtr = &it.second, builder] (const thread::Task &) -> bool {
			sourcePtr->data = openFontData(name, sourcePtr->params, [&] () -> FontData {
				if (sourcePtr->fontCallback) {
					return FontData(move(sourcePtr->fontCallback));
				} else if (!sourcePtr->fontExternalData.empty()) {
					return FontData(sourcePtr->fontExternalData, true);
				} else if (!sourcePtr->fontMemoryData.empty()) {
					return FontData(move(sourcePtr->fontMemoryData));
				} else if (!sourcePtr->fontFilePath.empty()) {
					auto d = filesystem::readIntoMemory<Interface>(sourcePtr->fontFilePath);
					if (!d.empty()) {
						return FontData(move(d));
					}
				}
				return FontData(BytesView(), false);
			});
			if (sourcePtr->data) {
				builder->onDataLoaded(true);
			} else {
				builder->onDataLoaded(false);
			}
			return true;
		});
	}

	builder->dynamicImage = Rc<gl::DynamicImage>::create([name = builder->builder.getName()] (gl::DynamicImage::Builder &builder) {
		builder.setImage(name,
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
	});

	_loop->compileImage(builder->dynamicImage, [this, builder] (bool success) {
		_loop->getApplication()->performOnMainThread([success, builder] {
			if (success) {
				builder->onImageLoaded(move(builder->dynamicImage));
			} else {
				builder->onImageLoaded(nullptr);
			}
		}, this);
	});

	return ret;
}

void FontLibrary::updateImage(const Rc<gl::DynamicImage> &image, Vector<font::FontUpdateRequest> &&data,
		Rc<renderqueue::DependencyEvent> &&dep) {
	if (!_active) {
		_pendingImageQueries.emplace_back(ImageQuery{image, move(data), move(dep)});
		return;
	}

	auto input = Rc<gl::RenderFontInput>::alloc();
	input->image = image;
	input->library = this;
	input->requests = move(data);
	/*input->output = [] (const gl::ImageInfo &info, BytesView data) {
		Bitmap bmp(data, info.extent.width, info.extent.height, bitmap::PixelFormat::A8);
		bmp.save(bitmap::FileFormat::Png, toString(Time::now().toMicros(), ".png"));
	};*/

	auto req = Rc<renderqueue::FrameRequest>::create(_queue);
	if (dep) {
		req->addSignalDependency(move(dep));
	}

	for (auto &it : _queue->getInputAttachments()) {
		req->addInput(it, move(input));
		break;
	}

	_loop->runRenderQueue(move(req));
}

uint16_t FontLibrary::getNextId() {
	for (uint32_t i = 1; i < _fontIds.size(); ++ i) {
		if (!_fontIds.test(i)) {
			_fontIds.set(i);
			return i;
		}
	}
	abort(); // active font limits exceeded
	return 0;
}

void FontLibrary::releaseId(uint16_t id) {
	_fontIds.reset(id);
}

Rc<FontFaceObjectHandle> FontLibrary::makeThreadHandle(const Rc<FontFaceObject> &obj) {
	std::shared_lock sharedLock(_sharedMutex);
	auto it = _threads.find(obj.get());
	if (it != _threads.end()) {
		auto iit = it->second.find(std::this_thread::get_id());
		if (iit != it->second.end()) {
			return iit->second;
		}
	}
	sharedLock.unlock();

	std::unique_lock uniqueLock(_sharedMutex);
	it = _threads.find(obj.get());
	if (it != _threads.end()) {
		auto iit = it->second.find(std::this_thread::get_id());
		if (iit != it->second.end()) {
			return iit->second;
		}
	}

	std::unique_lock<Mutex> lock(_mutex);
	auto face = newFontFace(obj->getData()->getView());
	lock.unlock();
	auto target = Rc<FontFaceObject>::create(obj->getName(), obj->getData(), face, obj->getSpec(), obj->getId());

	if (it == _threads.end()) {
		it = _threads.emplace(obj.get(), Map<std::thread::id, Rc<FontFaceObjectHandle>>()).first;
	}

	auto iit = it->second.emplace(std::this_thread::get_id(), Rc<FontFaceObjectHandle>::create(this, move(target), [this] (const FontFaceObjectHandle *obj) {
		std::unique_lock<Mutex> lock(_mutex);
		doneFontFace(obj->getFace());
	})).first->second;
	uniqueLock.unlock();
	return iit;
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

	for (auto &it : _pendingImageQueries) {
		updateImage(it.image, move(it.chars), move(it.dependency));
	}

	_pendingImageQueries.clear();
}

}
