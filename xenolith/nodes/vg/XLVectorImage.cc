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

#include "XLVectorImage.h"
#include "SPBitmap.h"
#include "SLSvgReader.h"

namespace stappler::xenolith {

void VectorCanvasResult::updateColor(const Color4F &color) {
	auto origin = Vec4(targetColor.r, targetColor.g, targetColor.b, targetColor.a);
	auto target = Vec4(color.r, color.g, color.b, color.a);
	for (auto &it : data) {
		auto data = Rc<gl::VertexData>::alloc();
		data->data = it.first->data;
		data->indexes = it.first->indexes;

		it.first = move(data);

		for (auto &iit : it.first->data) {
			iit.color = (iit.color / origin) * target;
		}
	}
}

bool VectorPath::init(VectorImage *image, const String &id, const Rc<Path> &path) {
	_image = image;
	_id = id;
	_path = path;
	return true;
}

bool VectorPath::init(VectorImage *image, const String &id, Rc<Path> &&path) {
	_image = image;
	_id = id;
	_path = move(path);
	return true;
}

size_t VectorPath::count() const {
	return _path ? _path->count() : 0;
}

VectorPath & VectorPath::moveTo(float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->moveTo(x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::lineTo(float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->lineTo(x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::quadTo(float x1, float y1, float x2, float y2) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->quadTo(x1, y1, x2, y2);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::cubicTo(float x1, float y1, float x2, float y2, float x3, float y3) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->cubicTo(x1, y1, x2, y2, x2, y3);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::arcTo(float rx, float ry, float rotation, bool largeFlag, bool sweepFlag, float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->arcTo(rx, ry, rotation, largeFlag, sweepFlag, x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::closePath() {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->closePath();
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::addRect(const Rect& rect) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addRect(rect);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::addOval(const Rect& oval) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addOval(oval);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::addCircle(float x, float y, float radius) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addCircle(x, y, radius);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::addArc(const Rect& oval, float startAngleInRadians, float sweepAngleInRadians) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addArc(oval, startAngleInRadians, sweepAngleInRadians);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::setFillColor(const Color4B &color) {
	if (_path && _path->getFillColor() == color) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setFillColor(color);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

const Color4B &VectorPath::getFillColor() const {
	return _path ? _path->getFillColor() : Color4B::BLACK;
}

VectorPath & VectorPath::setStrokeColor(const Color4B &color) {
	if (_path && _path->getStrokeColor() == color) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setStrokeColor(color);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

const Color4B &VectorPath::getStrokeColor() const {
	return _path ? _path->getStrokeColor() : Color4B::BLACK;
}

VectorPath & VectorPath::setFillOpacity(uint8_t value) {
	if (_path && _path->getFillOpacity() == value) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setFillOpacity(value);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}
uint8_t VectorPath::getFillOpacity() const {
	return _path ? _path->getFillOpacity() : 0;
}

VectorPath & VectorPath::setStrokeOpacity(uint8_t value) {
	if (_path && _path->getStrokeOpacity() == value) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setStrokeOpacity(value);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

uint8_t VectorPath::getStrokeOpacity() const {
	return _path ? _path->getStrokeOpacity() : 0;
}

VectorPath & VectorPath::setStrokeWidth(float width) {
	if (_path && _path->getStrokeWidth() == width) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setStrokeWidth(width);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

float VectorPath::getStrokeWidth() const {
	return _path ? _path->getStrokeWidth() : 0.0f;
}

VectorPath & VectorPath::setStyle(DrawStyle s) {
	if (_path && _path->getStyle() == s) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setStyle(s);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath::DrawStyle VectorPath::getStyle() const {
	return _path ? _path->getStyle() : DrawStyle::FillAndStroke;
}

VectorPath & VectorPath::setTransform(const Mat4 &t) {
	if (_path && _path->getTransform() == t) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setTransform(t);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPath & VectorPath::applyTransform(const Mat4 &t) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->applyTransform(t);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

const Mat4 &VectorPath::getTransform() const {
	return _path ? _path->getTransform() : Mat4::IDENTITY;
}

VectorPath & VectorPath::setAntialiased(bool value) {
	if (_path && _path->isAntialiased() == value) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setAntialiased(value);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

bool VectorPath::isAntialiased() const {
	return _path ? _path->isAntialiased() : false;
}

VectorPath & VectorPath::clear() {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->clear();
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

StringView VectorPath::getId() const {
	return _id;
}

bool VectorPath::empty() const {
	return _path ? _path->empty() : true;
}

bool VectorPath::valid() const {
	return _path && _image;
}

VectorPath::operator bool() const {
	return valid() && !empty();
}

void VectorPath::setPath(Rc<Path> &&path) {
	_path = move(path);
	_copyOnWrite = false;
}

VectorPath::Path *VectorPath::getPath() const {
	return _path;
}

void VectorPath::markCopyOnWrite() {
	_copyOnWrite = true;
}

void VectorPath::setImage(VectorImage *image) {
	_image = image;
}

void VectorPath::copy() {
	if (_copyOnWrite) {
		if (_image) {
			_path = _image->copyPath(_id, _path);
		}
		_copyOnWrite = false;
	}
}

bool VectorImageData::init(VectorImage *image, Size size, Rect viewBox, Vector<layout::PathXRef> &&order, Map<String, layout::Path> &&paths, uint16_t ids) {
	_imageSize = size;
	_image = image;

	if (!viewBox.equals(Rect::ZERO)) {
		const float scaleX = _imageSize.width / viewBox.size.width;
		const float scaleY = _imageSize.height / viewBox.size.height;
		_viewBoxTransform = Mat4::IDENTITY;
		_viewBoxTransform.scale(scaleX, scaleY, 1.0f);
		_viewBoxTransform.translate(-viewBox.origin.x, -viewBox.origin.y, 0.0f);
		_viewBox = Rect(viewBox.origin.x * scaleX, viewBox.origin.y * scaleY,
				viewBox.size.width * scaleX, viewBox.size.height * scaleY);
	} else {
		_viewBox = Rect(0, 0, _imageSize.width, _imageSize.height);
	}

	_nextId = ids;
	_order = move(order);

	for (auto &it : paths) {
		_paths.emplace(it.first, Rc<Path>::alloc(move(it.second)));
	}

	return true;
}

bool VectorImageData::init(VectorImage *image, Size size, Rect viewBox) {
	_imageSize = size;
	_image = image;
	_viewBox = viewBox;
	return true;
}

bool VectorImageData::init(VectorImageData &data) {
	_isAntialiased = data._isAntialiased;
	_allowBatchDrawing = data._allowBatchDrawing;
	_imageSize = data._imageSize;
	_viewBox = data._viewBox;
	_viewBoxTransform = data._viewBoxTransform;
	_order = data._order;
	_paths = data._paths;
	_nextId = data._nextId;
	_image = data._image;
	return true;
}

const Map<String, Rc<VectorImageData::Path>> &VectorImageData::getPaths() const {
	return _paths;
}

Rc<VectorImageData::Path> VectorImageData::copyPath(StringView str, const Rc<Path> &path) {
	auto it = _paths.find(str);
	if (it != _paths.end()) {
		it->second = Rc<Path>::alloc(*it->second);
		return it->second;
	}
	return nullptr;
}

uint16_t VectorImageData::getNextId() {
	auto ret = _nextId;
	++ _nextId;
	return ret;
}

Rc<VectorImageData::Path> VectorImageData::addPath(StringView id, Path &&path, Mat4 mat) {
	Rc<VectorImageData::Path> ret;
	auto it = _paths.find(id);
	if (it == _paths.end()) {
		ret = _paths.emplace(id.str(), Rc<Path>::alloc(move(path))).first->second;
		_order.emplace_back(layout::PathXRef{id.str(), mat});
	} else {
		ret = it->second = Rc<Path>::alloc(move(path));
		bool found = false;
		for (auto &iit : _order) {
			if (iit.id == id) {
				iit.mat = mat;
				found = true;
			}
		}
		if (!found) {
			_order.emplace_back(layout::PathXRef{id.str(), mat});
		}
	}

	return ret;
}

void VectorImageData::removePath(StringView id) {
	auto it = _paths.find(id);
	if (it != _paths.end()) {
		_paths.erase(it);
	}

	auto iit = _order.begin();
	while (iit != _order.end()) {
		if (iit->id == id) {
			iit = _order.erase(iit);
		} else {
			++ iit;
		}
	}
}

void VectorImageData::clear() {
	_paths.clear();
	_order.clear();
}

void VectorImageData::resetDrawOrder() {
	_order.clear();
	for (auto &it : _paths) {
		_order.emplace_back(layout::PathXRef{it.first});
	}
}

bool VectorImage::isSvg(StringView str) {
	return Bitmap::check(Bitmap::FileFormat::Svg, (const uint8_t *)str.data(), str.size());
}

bool VectorImage::isSvg(BytesView data) {
	return Bitmap::check(Bitmap::FileFormat::Svg, data.data(), data.size());
}

bool VectorImage::isSvg(FilePath file) {
	auto d = filesystem::readIntoMemory(file.get(), 0, 512);
	return Bitmap::check(Bitmap::FileFormat::Svg, d.data(), d.size());
}

VectorImage::~VectorImage() {
	for (auto &it : _paths) {
		it.second->setImage(nullptr);
	}
}

bool VectorImage::init(Size size, StringView data) {
	Path path;
	if (!path.init(data)) {
		return false;
	}
	return init(size, std::move(path));
}

bool VectorImage::init(Size size, Path && path) {
	_data = Rc<VectorImageData>::create(this, size, Rect(0, 0, size.width, size.height));
	addPath(move(path));
	return true;
}

bool VectorImage::init(Size size) {
	_data = Rc<VectorImageData>::create(this, size, Rect(0, 0, size.width, size.height));
	return true;
}

bool VectorImage::init(StringView data) {
	String tmp = data.str();
	layout::SvgReader reader;
	html::parse<layout::SvgReader, StringView, layout::SvgTag>(reader, StringView(tmp));

	if (!reader._paths.empty()) {
		_data = Rc<VectorImageData>::create(this, Size(reader._width, reader._height), reader._viewBox,
				move(reader._drawOrder), move(reader._paths), reader._nextId);
		for (auto &it : _data->getPaths()) {
			_paths.emplace(it.first, Rc<VectorPath>::create(this, it.first, it.second));
		}
		return true;
	} else {
		log::text("layout::Image", "No paths found in input string");
	}

	return false;
}

bool VectorImage::init(BytesView data) {
	layout::SvgReader reader;
	html::parse<layout::SvgReader, StringView, layout::SvgTag>(reader, StringView((const char *)data.data(), data.size()));

	if (!reader._paths.empty()) {
		_data = Rc<VectorImageData>::create(this, Size(reader._width, reader._height), reader._viewBox,
				move(reader._drawOrder), move(reader._paths), reader._nextId);
		for (auto &it : _data->getPaths()) {
			_paths.emplace(it.first, Rc<VectorPath>::create(this, it.first, it.second));
		}
		return true;
	} else {
		log::text("layout::Image", "No paths found in input data");
	}

	return false;
}

bool VectorImage::init(FilePath path) {
	return init(filesystem::readTextFile(path.get()));
}

Size VectorImage::getImageSize() const {
	return _data->getImageSize();
}

Rect VectorImage::getViewBox() const {
	return _data->getViewBox();
}

Rc<VectorPath> VectorImage::addPath(const Path &path, StringView tag, Mat4 vec) {
	return addPath(Path(path), tag, vec);
}

Rc<VectorPath> VectorImage::addPath(Path &&path, StringView tag, Mat4 vec) {
	if (_copyOnWrite) {
		copy();
	}

	String idStr;
	if (tag.empty()) {
		idStr = toString("auto-", _data->getNextId());
		tag = idStr;
	}

	auto pathObj = _data->addPath(tag, move(path), vec);

	setDirty();

	auto it = _paths.find(tag);
	if (it == _paths.end()) {
		auto obj = Rc<VectorPath>::create(this, tag.str(), move(pathObj));
		return _paths.emplace(idStr.empty() ? tag.str() : move(idStr), move(obj)).first->second;
	} else {
		it->second->setPath(move(pathObj));
		return it->second;
	}
}

Rc<VectorPath> VectorImage::addPath(StringView tag, Mat4 vec) {
	return addPath(Path(), tag, vec);
}

Rc<VectorPath> VectorImage::getPath(StringView tag) const {
	auto it = _paths.find(tag);
	if (it != _paths.end()) {
		return it->second;
	}
	return nullptr;
}

void VectorImage::removePath(const Rc<VectorPath> &path) {
	removePath(path->getId());
}

void VectorImage::removePath(StringView tag) {
	if (_copyOnWrite) {
		copy();
	}

	_data->removePath(tag);
	auto it = _paths.find(tag);
	if (it != _paths.end()) {
		it->second->setImage(nullptr);
		_paths.erase(it);
	}
	setDirty();
}

void VectorImage::clear() {
	if (_copyOnWrite) {
		copy();
	}

	_data->clear();

	for (auto &it : _paths) {
		it.second->setImage(nullptr);
	}
	_paths.clear();
	setDirty();
}

const Vector<layout::PathXRef> &VectorImage::getDrawOrder() const {
	return _data->getDrawOrder();
}

void VectorImage::setDrawOrder(const Vector<layout::PathXRef> &vec) {
	if (_copyOnWrite) {
		copy();
	}

	_data->setDrawOrder(Vector<layout::PathXRef>(vec));
	setDirty();
}

void VectorImage::setDrawOrder(Vector<layout::PathXRef> &&vec) {
	if (_copyOnWrite) {
		copy();
	}

	_data->setDrawOrder(move(vec));
	setDirty();
}

void VectorImage::resetDrawOrder() {
	if (_copyOnWrite) {
		copy();
	}

	_data->resetDrawOrder();
	setDirty();
}

void VectorImage::setViewBoxTransform(const Mat4 &m) {
	if (_data->getViewBoxTransform() == m) {
		return;
	}

	if (_copyOnWrite) {
		copy();
	}

	_data->setViewBoxTransform(m);
	setDirty();
}

const Mat4 &VectorImage::getViewBoxTransform() const {
	return _data->getViewBoxTransform();
}

void VectorImage::setAntialiased(bool value) {
	if (_data->isAntialiased() == value) {
		return;
	}

	if (_copyOnWrite) {
		copy();
	}

	_data->setAntialiased(value);
}

bool VectorImage::isAntialiased() const {
	return _data->isAntialiased();
}

void VectorImage::setBatchDrawing(bool value) {
	if (_data->isBatchDrawing() == value) {
		return;
	}

	if (_copyOnWrite) {
		copy();
	}

	_data->setBatchDrawing(value);
}

bool VectorImage::isBatchDrawing() const {
	return _data->isBatchDrawing();
}

Rc<VectorImageData> VectorImage::popData() {
	markCopyOnWrite();
	return _data;
}

bool VectorImage::isDirty() const {
	return _dirty;
}

void VectorImage::setDirty() {
	_dirty = true;
}

void VectorImage::clearDirty() {
	_dirty = false;
}

void VectorImage::copy() {
	if (_copyOnWrite) {
		_data = Rc<VectorImageData>::create(*_data.get());
		_copyOnWrite = false;
	}
}

void VectorImage::markCopyOnWrite() {
	_copyOnWrite = true;
	for (auto &it : _paths) {
		it.second->markCopyOnWrite();
	}
}

Rc<VectorImage::Path> VectorImage::copyPath(StringView str, const Rc<Path> &path) {
	copy();
	return _data->copyPath(str, path);
}

}
