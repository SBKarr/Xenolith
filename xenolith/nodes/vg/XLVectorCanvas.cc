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

#include "XLVectorCanvas.h"
#include "SPTess.h"

namespace stappler::xenolith {

struct VectorCanvasPathOutput {
	Color4F color;
	gl::VertexData *vertexes = nullptr;
	uint32_t material = 0;
	uint32_t objects = 0;
};

struct VectorCanvasPathDrawer {
	const VectorPath *path = nullptr;

	float quality = 0.5f; // approximation level (more is better)
	Color4F originalColor;

	uint32_t draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform, gl::VertexData *, bool cache);
};

struct VectorCanvasCacheData {
	Rc<gl::VertexData> data;
	String name;
	float quality = 1.0f;
	float scale = 1.0f;

	bool operator< (const VectorCanvasCacheData &other) const {
		if (name != other.name) {
			return name < other.name;
		} else if (quality != other.quality) {
			return quality < other.quality;
		} else {
			return scale < other.scale;
		}
	}
};

struct VectorCanvas::Data : memory::AllocPool {
	memory::pool_t *pool = nullptr;
	memory::pool_t *transactionPool = nullptr;
	bool isOwned = true;

	VectorCanvasPathDrawer pathDrawer;

	Mat4 transform;
	Vector<Mat4> states;

	TimeInterval subAccum;

	Rc<VectorImageData> image;
	Size2 targetSize;

	Vector<Pair<Mat4, Rc<gl::VertexData>>> *out = nullptr;
	Set<VectorCanvasCacheData> cacheData;

	Data(memory::pool_t *p) : pool(p) {
		transactionPool = memory::pool::create(pool);

		loadCache();
	}

	~Data() {
		saveCache();
	}

	void save() {
		states.push_back(transform);
	}

	void restore() {
		if (!states.empty()) {
			transform = states.back();
			states.pop_back();
		}
	}

	void applyTransform(const Mat4 &t) {
		transform *= t;
	}

	void draw(const VectorPath &, StringView cache);
	void draw(const VectorPath &, StringView cache, const Mat4 &);

	void doDraw(const VectorPath &, StringView cache);

	void writeCacheData(const VectorPath &p, gl::VertexData *out, const gl::VertexData &source);

	void loadCache();
	void saveCache();
};

static void VectorCanvasPathDrawer_pushVertex(void *ptr, uint32_t idx, const Vec2 &pt, float vertexValue) {
	auto out = (VectorCanvasPathOutput *)ptr;
	if (size_t(idx) >= out->vertexes->data.size()) {
		out->vertexes->data.resize(idx + 1);
	}

	out->vertexes->data[idx] = gl::Vertex_V4F_V4F_T2F2U{
		Vec4(pt, 0.0f, 1.0f),
		Vec4(out->color.r, out->color.g, out->color.b, out->color.a * vertexValue),
		Vec2(0.0f, 0.0f), out->material, 0
	};

	std::cout << "Vertex: " << idx << ": " << pt << "\n";
}

static void VectorCanvasPathDrawer_pushTriangle(void *ptr, uint32_t pt[3]) {
	auto out = (VectorCanvasPathOutput *)ptr;
	out->vertexes->indexes.emplace_back(pt[0]);
	out->vertexes->indexes.emplace_back(pt[1]);
	out->vertexes->indexes.emplace_back(pt[2]);
	++ out->objects;

	std::cout << "Face: " << pt[0] << " " << pt[1] << " " << pt[2] << "\n";
}

Rc<VectorCanvas> VectorCanvas::getInstance() {
	static thread_local Rc<VectorCanvas> tl_instance = nullptr;
	if (!tl_instance) {
		tl_instance = Rc<VectorCanvas>::create();
	}
	return tl_instance;
}

VectorCanvas::~VectorCanvas() {
	if (_data && _data->isOwned) {
		auto p = _data->pool;
		_data->~Data();
		_data = nullptr;
		memory::pool::destroy(p);
	}
}

bool VectorCanvas::init(float quality, Color4F color) {
	auto p = memory::pool::createTagged("xenolith::VectorCanvas");
	_data = new (p) Data(p);
	if (_data) {
		_data->pathDrawer.quality = quality;
		_data->pathDrawer.originalColor = color;
		return true;
	}
	return false;
}

void VectorCanvas::setColor(Color4F color) {
	_data->pathDrawer.originalColor = color;
}

Color4F VectorCanvas::getColor() const {
	return _data->pathDrawer.originalColor;
}

void VectorCanvas::setQuality(float value) {
	_data->pathDrawer.quality = value;
}

float VectorCanvas::getQuality() const {
	return _data->pathDrawer.quality;
}

Rc<VectorCanvasResult> VectorCanvas::draw(Rc<VectorImageData> &&image, Size2 targetSize) {
	auto ret = Rc<VectorCanvasResult>::alloc();
	_data->out = &ret->data;
	_data->image = move(image);
	ret->targetSize = _data->targetSize = targetSize;
	ret->targetColor = getColor();

	auto imageSize = _data->image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(targetSize.width / imageSize.width, targetSize.height / imageSize.height, 1.0f);

	auto &m = _data->image->getViewBoxTransform();
	if (!m.isIdentity()) {
		t *= m;
	}

	bool isIdentity = t.isIdentity();

	if (!isIdentity) {
		_data->save();
		_data->applyTransform(t);
	}

	_data->image->draw([&] (const VectorPath &path, StringView cacheId, const Mat4 &pos) {
		if (pos.isIdentity()) {
			_data->draw(path, cacheId);
		} else {
			_data->draw(path, cacheId, pos);
		}
	});

	if (!isIdentity) {
		_data->restore();
	}

	if (!_data->out->empty() && _data->out->back().second->data.empty()) {
		_data->out->pop_back();
	}

	size_t nverts = 0;
	size_t ntriangles = 0;

	for (auto &it : (*_data->out)) {
		nverts += it.second->data.size();
		ntriangles += it.second->indexes.size() / 3;
	}

	std::cout << "Q: " << _data->pathDrawer.quality << "; Vertexes: " << nverts << "; Triangles: " << ntriangles << "\n";

	_data->out = nullptr;
	_data->image = nullptr;
	return ret;
}

void VectorCanvas::Data::draw(const VectorPath &path, StringView cache) {
	bool hasTransform = !path.getTransform().isIdentity();
	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path, cache);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::draw(const VectorPath &path, StringView cache, const Mat4 &mat) {
	auto matTransform = path.getTransform() * mat;
	bool hasTransform = !matTransform.isIdentity();

	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path, cache);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::doDraw(const VectorPath &path, StringView cache) {
	gl::VertexData *outData = nullptr;
	if (out->empty() || !out->back().second->data.empty()) {
		out->emplace_back(transform, Rc<gl::VertexData>::alloc());
	}

	outData = out->back().second.get();
	memory::pool::push(transactionPool);

	do {
		if (!cache.empty()) {
			float quality = pathDrawer.quality;

			Vec3 scaleVec; transform.getScale(&scaleVec);
			float scale = std::max(scaleVec.x, scaleVec.y);

			VectorCanvasCacheData data{nullptr, cache.str<Interface>(), quality, scale };

			auto it = cacheData.find(data);
			if (it != cacheData.end()) {
				if (!it->data->indexes.empty()) {
					writeCacheData(path, outData, *it->data);
				}
				break;
			}

			data.data = Rc<gl::VertexData>::alloc();

			auto ret = pathDrawer.draw(transactionPool, path, transform, data.data, true);
			if (ret != 0) {
				auto it = cacheData.emplace(move(data)).first;
				writeCacheData(path, outData, *it->data);
			} else {
				outData->data.clear();
				outData->indexes.clear();
				out->back().first = transform;
			}
		} else {
			auto ret = pathDrawer.draw(transactionPool, path, transform, outData, false);
			if (ret == 0) {
				outData->data.clear();
				outData->indexes.clear();
				out->back().first = transform;
			}
		}
	} while (0);

	memory::pool::pop();
	memory::pool::clear(transactionPool);
}

void VectorCanvas::Data::writeCacheData(const VectorPath &p, gl::VertexData *out, const gl::VertexData &source) {
	auto fillColor = pathDrawer.originalColor * p.getFillColor();
	auto strokeColor = pathDrawer.originalColor * p.getStrokeColor();

	Vec4 fillVec = fillColor;
	Vec4 strokeVec = strokeColor;

	out->indexes = source.indexes;
	out->data = source.data;
	for (auto &it : out->data) {
		if (it.material == 0) {
			it.color = it.color * fillVec;
		} else if (it.material == 1) {
			it.color = it.color * strokeVec;
		}
	}
}

void VectorCanvas::Data::loadCache() {
	auto path = filesystem::writablePath<Interface>("vector_cache.cbor");

	if (filesystem::exists(path)) {
		auto val = data::readFile<Interface>(path);
		for (auto &it : val.asArray()) {
			VectorCanvasCacheData data;
			data.name = it.getString("name");
			data.quality = it.getDouble("quality");
			data.scale = it.getDouble("scale");

			auto &vertexes = it.getBytes("vertexes");
			auto &indexes = it.getBytes("indexes");

			data.data = Rc<gl::VertexData>::alloc();
			data.data->data.assign((gl::Vertex_V4F_V4F_T2F2U *)vertexes.data(),
					(gl::Vertex_V4F_V4F_T2F2U *)(vertexes.data() + vertexes.size()));
			data.data->indexes.assign((uint32_t *)indexes.data(),
					(uint32_t *)(indexes.data() + indexes.size()));

			cacheData.emplace(move(data));
		}
	}
}

void VectorCanvas::Data::saveCache() {
	Value val;
	for (auto &it : cacheData) {
		if (!it.data) {
			continue;
		}

		Value data;
		data.setString(it.name, "name");
		data.setDouble(it.quality, "quality");
		data.setDouble(it.scale, "scale");

		data.setBytes(BytesView((uint8_t *)it.data->data.data(), it.data->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U)), "vertexes");
		data.setBytes(BytesView((uint8_t *)it.data->indexes.data(), it.data->indexes.size() * sizeof(uint32_t)), "indexes");

		val.addValue(move(data));
	}

	if (!val.empty()) {
		auto path = filesystem::writablePath<Interface>("vector_cache.cbor");
		filesystem::mkdir(filepath::root(path));

		filesystem::remove(path);
		data::save(val, path, data::EncodeFormat::Cbor);
	}
}

uint32_t VectorCanvasPathDrawer::draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform,
		gl::VertexData *out, bool cache) {
	path = &p;

	float approxScale = 1.0f;
	auto style = path->getStyle();

	Rc<geom::Tesselator> strokeTess = ((style & geom::DrawStyle::Stroke) != geom::DrawStyle::None) ? Rc<geom::Tesselator>::create(pool) : nullptr;
	Rc<geom::Tesselator> fillTess = ((style & geom::DrawStyle::Fill) != geom::DrawStyle::None) ? Rc<geom::Tesselator>::create(pool) : nullptr;

	geom::LineDrawer line(approxScale * quality, Rc<geom::Tesselator>(fillTess), Rc<geom::Tesselator>(strokeTess),
			path->getStrokeWidth());

	Vec3 scale; transform.getScale(&scale);
	approxScale = std::max(scale.x, scale.y);

	auto d = path->getPoints().data();
	for (auto &it : path->getCommands()) {
		switch (it) {
		case VectorPath::Command::MoveTo: line.drawBegin(d[0].p.x, d[0].p.y); ++ d; break;
		case VectorPath::Command::LineTo: line.drawLine(d[0].p.x, d[0].p.y); ++ d; break;
		case VectorPath::Command::QuadTo: line.drawQuadBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y); d += 2; break;
		case VectorPath::Command::CubicTo: line.drawCubicBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y, d[2].p.x, d[2].p.y); d += 3; break;
		case VectorPath::Command::ArcTo: line.drawArc(d[0].p.x, d[0].p.y, d[2].f.v, d[2].f.a, d[2].f.b, d[1].p.x, d[1].p.y); d += 3; break;
		case VectorPath::Command::ClosePath: line.drawClose(true); break;
		default: break;
		}
	}

	line.drawClose(false);

	VectorCanvasPathOutput target { Color4F::WHITE, out };
	geom::TessResult result;
	result.target = &target;
	result.pushVertex = VectorCanvasPathDrawer_pushVertex;
	result.pushTriangle = VectorCanvasPathDrawer_pushTriangle;

	if (fillTess) {
		if (path->isAntialiased() && (path->getStyle() == vg::DrawStyle::Fill || path->getStrokeOpacity() < 96)) {
			fillTess->setAntialiasValue(config::VGAntialiasFactor * approxScale);
		}
		fillTess->setWindingRule(path->getWindingRule());
		fillTess->prepare(result);
	}

	if (strokeTess) {
		if (path->isAntialiased()) {
			strokeTess->setAntialiasValue(config::VGAntialiasFactor * approxScale);
		}

		strokeTess->setWindingRule(vg::Winding::EvenOdd);
		strokeTess->prepare(result);
	}

	out->data.resize(result.nvertexes);
	out->indexes.reserve(result.nfaces * 3);

	if (fillTess) {
		target.material = 0;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = originalColor * path->getFillColor();
		}
		fillTess->write(result);
	}

	if (strokeTess) {
		target.material = 1;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = originalColor * path->getStrokeColor();
		}
		strokeTess->write(result);
	}

	return target.objects;
}

SP_EXTERN_C void tessLog(const char *msg) {
	stappler::log::text("XL-Tess", msg);
}

}
