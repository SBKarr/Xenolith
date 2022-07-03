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
#include "XLSvgReader.h"

namespace stappler::xenolith {

void VectorCanvasResult::updateColor(const Color4F &color) {
	auto target = Vec4(color.r, color.g, color.b, color.a) / Vec4(targetColor.r, targetColor.g, targetColor.b, targetColor.a);
	for (auto &it : data) {
		auto data = Rc<gl::VertexData>::alloc();
		data->data = it.second->data;
		data->indexes = it.second->indexes;

		it.second = move(data);

		for (auto &iit : it.second->data) {
			iit.color = iit.color * target;
		}
	}
	targetColor = Color4F(color.r, color.g, color.b, color.a);
}

bool VectorPathRef::init(VectorImage *image, const String &id, const Rc<VectorPath> &path) {
	_image = image;
	_id = id;
	_path = path;
	return true;
}

bool VectorPathRef::init(VectorImage *image, const String &id, Rc<VectorPath> &&path) {
	_image = image;
	_id = id;
	_path = move(path);
	return true;
}

size_t VectorPathRef::count() const {
	return _path ? _path->count() : 0;
}

VectorPathRef & VectorPathRef::moveTo(float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->moveTo(x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::lineTo(float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->lineTo(x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::quadTo(float x1, float y1, float x2, float y2) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->quadTo(x1, y1, x2, y2);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::cubicTo(float x1, float y1, float x2, float y2, float x3, float y3) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->cubicTo(x1, y1, x2, y2, x2, y3);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::arcTo(float rx, float ry, float rotation, bool largeFlag, bool sweepFlag, float x, float y) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->arcTo(rx, ry, rotation, largeFlag, sweepFlag, x, y);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::closePath() {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->closePath();
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::addRect(const Rect& rect) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addRect(rect);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::addOval(const Rect& oval) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addOval(oval);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::addCircle(float x, float y, float radius) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addCircle(x, y, radius);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::addArc(const Rect& oval, float startAngleInRadians, float sweepAngleInRadians) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->addArc(oval, startAngleInRadians, sweepAngleInRadians);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

VectorPathRef & VectorPathRef::setFillColor(const Color4B &color) {
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

const Color4B &VectorPathRef::getFillColor() const {
	return _path ? _path->getFillColor() : Color4B::BLACK;
}

VectorPathRef & VectorPathRef::setStrokeColor(const Color4B &color) {
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

const Color4B &VectorPathRef::getStrokeColor() const {
	return _path ? _path->getStrokeColor() : Color4B::BLACK;
}

VectorPathRef & VectorPathRef::setFillOpacity(uint8_t value) {
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
uint8_t VectorPathRef::getFillOpacity() const {
	return _path ? _path->getFillOpacity() : 0;
}

VectorPathRef & VectorPathRef::setStrokeOpacity(uint8_t value) {
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

uint8_t VectorPathRef::getStrokeOpacity() const {
	return _path ? _path->getStrokeOpacity() : 0;
}

VectorPathRef & VectorPathRef::setStrokeWidth(float width) {
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

float VectorPathRef::getStrokeWidth() const {
	return _path ? _path->getStrokeWidth() : 0.0f;
}

VectorPathRef &VectorPathRef::setWindingRule(vg::Winding value) {
	if (_path && _path->getWindingRule() == value) {
		return *this;
	}

	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->setWindingRule(value);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

vg::Winding VectorPathRef::getWindingRule() const {
	return _path ? _path->getWindingRule() : vg::Winding::NonZero;
}

VectorPathRef & VectorPathRef::setStyle(vg::DrawStyle s) {
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

vg::DrawStyle VectorPathRef::getStyle() const {
	return _path ? _path->getStyle() : vg::DrawStyle::FillAndStroke;
}

VectorPathRef & VectorPathRef::setTransform(const Mat4 &t) {
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

VectorPathRef & VectorPathRef::applyTransform(const Mat4 &t) {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->applyTransform(t);
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

const Mat4 &VectorPathRef::getTransform() const {
	return _path ? _path->getTransform() : Mat4::IDENTITY;
}

VectorPathRef & VectorPathRef::setAntialiased(bool value) {
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

bool VectorPathRef::isAntialiased() const {
	return _path ? _path->isAntialiased() : false;
}

VectorPathRef & VectorPathRef::clear() {
	if (_copyOnWrite) {
		copy();
	}

	if (_path) {
		_path->clear();
		if (_image) { _image->setDirty(); }
	}
	return *this;
}

StringView VectorPathRef::getId() const {
	return _id;
}

bool VectorPathRef::empty() const {
	return _path ? _path->empty() : true;
}

bool VectorPathRef::valid() const {
	return _path && _image;
}

VectorPathRef::operator bool() const {
	return valid() && !empty();
}

void VectorPathRef::setPath(Rc<VectorPath> &&path) {
	_path = move(path);
	_copyOnWrite = false;
}

VectorPath *VectorPathRef::getPath() const {
	return _path;
}

void VectorPathRef::markCopyOnWrite() {
	_copyOnWrite = true;
}

void VectorPathRef::setImage(VectorImage *image) {
	_image = image;
}

void VectorPathRef::copy() {
	if (_copyOnWrite) {
		if (_image) {
			_path = _image->copyPath(_id, _path);
		}
		_copyOnWrite = false;
	}
}

bool VectorImageData::init(VectorImage *image, Size2 size, Rect viewBox, Vector<vg::PathXRef> &&order, Map<String, VectorPath> &&paths, uint16_t ids) {
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
		_paths.emplace(it.first, Rc<VectorPath>::alloc(move(it.second)));
	}

	return true;
}

bool VectorImageData::init(VectorImage *image, Size2 size, Rect viewBox) {
	_imageSize = size;
	_image = image;
	_viewBox = viewBox;
	return true;
}

bool VectorImageData::init(VectorImageData &data) {
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

void VectorImageData::setImageSize(const Size2 &size) {
	_imageSize = size;
}

const Map<String, Rc<VectorPath>> &VectorImageData::getPaths() const {
	return _paths;
}

Rc<VectorPath> VectorImageData::copyPath(StringView str, const Rc<VectorPath> &path) {
	auto it = _paths.find(str);
	if (it != _paths.end()) {
		it->second = Rc<VectorPath>::alloc(*it->second);
		return it->second;
	}
	return nullptr;
}

uint16_t VectorImageData::getNextId() {
	auto ret = _nextId;
	++ _nextId;
	return ret;
}

Rc<VectorPath> VectorImageData::addPath(StringView id, StringView cache, VectorPath &&path, Mat4 mat) {
	Rc<VectorPath> ret;
	auto it = _paths.find(id);
	if (it == _paths.end()) {
		ret = _paths.emplace(id.str<Interface>(), Rc<VectorPath>::alloc(move(path))).first->second;
		_order.emplace_back(vg::PathXRef{id.str<Interface>(), cache.str<Interface>(), mat});
	} else {
		ret = it->second = Rc<VectorPath>::alloc(move(path));
		bool found = false;
		for (auto &iit : _order) {
			if (iit.id == id) {
				iit.mat = mat;
				found = true;
			}
		}
		if (!found) {
			_order.emplace_back(vg::PathXRef{id.str<Interface>(), cache.str<Interface>(), mat});
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
		_order.emplace_back(vg::PathXRef{it.first});
	}
}

bool VectorImage::isSvg(StringView str) {
	return bitmap::check(bitmap::FileFormat::Svg, (const uint8_t *)str.data(), str.size());
}

bool VectorImage::isSvg(BytesView data) {
	return bitmap::check(bitmap::FileFormat::Svg, data.data(), data.size());
}

bool VectorImage::isSvg(FilePath file) {
	auto d = filesystem::readIntoMemory<Interface>(file.get(), 0, 512);
	return bitmap::check(bitmap::FileFormat::Svg, d.data(), d.size());
}

VectorImage::~VectorImage() {
	for (auto &it : _paths) {
		it.second->setImage(nullptr);
	}
}

bool VectorImage::init(Size2 size, StringView data) {
	VectorPath path;
	if (!path.init(data)) {
		return false;
	}
	return init(size, std::move(path));
}

bool VectorImage::init(Size2 size, VectorPath && path) {
	_data = Rc<VectorImageData>::create(this, size, Rect(0, 0, size.width, size.height));
	addPath(move(path));
	return true;
}

bool VectorImage::init(Size2 size) {
	_data = Rc<VectorImageData>::create(this, size, Rect(0, 0, size.width, size.height));
	return true;
}

bool VectorImage::init(StringView data) {
	String tmp = data.str<Interface>();
	vg::SvgReader reader;
	html::parse<vg::SvgReader, StringView, vg::SvgTag>(reader, StringView(tmp));

	if (!reader._paths.empty()) {
		_data = Rc<VectorImageData>::create(this, Size2(reader._width, reader._height), reader._viewBox,
				move(reader._drawOrder), move(reader._paths), reader._nextId);
		for (auto &it : _data->getPaths()) {
			_paths.emplace(it.first, Rc<VectorPathRef>::create(this, it.first, it.second));
		}
		return true;
	} else {
		log::text("layout::Image", "No paths found in input string");
	}

	return false;
}

bool VectorImage::init(BytesView data) {
	vg::SvgReader reader;
	html::parse<vg::SvgReader, StringView, vg::SvgTag>(reader, StringView((const char *)data.data(), data.size()));

	if (!reader._paths.empty()) {
		_data = Rc<VectorImageData>::create(this, Size2(reader._width, reader._height), reader._viewBox,
				move(reader._drawOrder), move(reader._paths), reader._nextId);
		for (auto &it : _data->getPaths()) {
			_paths.emplace(it.first, Rc<VectorPathRef>::create(this, it.first, it.second));
		}
		return true;
	} else {
		log::text("layout::Image", "No paths found in input data");
	}

	return false;
}

bool VectorImage::init(FilePath path) {
	return init(filesystem::readTextFile<Interface>(path.get()));
}

void VectorImage::setImageSize(const Size2 &size) {
	if (size == _data->getImageSize()) {
		return;
	}

	if (_copyOnWrite) {
		copy();
	}

	_data->setImageSize(size);
}

Size2 VectorImage::getImageSize() const {
	return _data->getImageSize();
}

Rect VectorImage::getViewBox() const {
	return _data->getViewBox();
}

Rc<VectorPathRef> VectorImage::addPath(const VectorPath &path, StringView tag, StringView cache, Mat4 vec) {
	return addPath(VectorPath(path), tag, cache, vec);
}

Rc<VectorPathRef> VectorImage::addPath(VectorPath &&path, StringView tag, StringView cache, Mat4 vec) {
	if (_copyOnWrite) {
		copy();
	}

	String idStr;
	if (tag.empty()) {
		idStr = toString("auto-", _data->getNextId());
		tag = idStr;
	}

	auto pathObj = _data->addPath(tag, cache, move(path), vec);

	setDirty();

	auto it = _paths.find(tag);
	if (it == _paths.end()) {
		auto obj = Rc<VectorPathRef>::create(this, tag.str<Interface>(), move(pathObj));
		return _paths.emplace(idStr.empty() ? tag.str<Interface>() : move(idStr), move(obj)).first->second;
	} else {
		it->second->setPath(move(pathObj));
		return it->second;
	}
}

Rc<VectorPathRef> VectorImage::addPath(StringView tag, StringView cache, Mat4 vec) {
	return addPath(VectorPath(), tag, cache, vec);
}

Rc<VectorPathRef> VectorImage::getPath(StringView tag) const {
	auto it = _paths.find(tag);
	if (it != _paths.end()) {
		return it->second;
	}
	return nullptr;
}

void VectorImage::removePath(const Rc<VectorPathRef> &path) {
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

const Vector<vg::PathXRef> &VectorImage::getDrawOrder() const {
	return _data->getDrawOrder();
}

void VectorImage::setDrawOrder(const Vector<vg::PathXRef> &vec) {
	if (_copyOnWrite) {
		copy();
	}

	_data->setDrawOrder(Vector<vg::PathXRef>(vec));
	setDirty();
}

void VectorImage::setDrawOrder(Vector<vg::PathXRef> &&vec) {
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

Rc<VectorPath> VectorImage::copyPath(StringView str, const Rc<VectorPath> &path) {
	copy();
	return _data->copyPath(str, path);
}

}
