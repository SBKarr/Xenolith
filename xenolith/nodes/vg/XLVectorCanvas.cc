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
#include "XLTesselator.h"

namespace stappler::xenolith {

struct VectorCanvasPathOutput {
	gl::VertexData *vertexes;
	Color4F color;
	uint32_t objects;
};

struct VectorCanvasPathDrawer {
	TESSalloc tessAlloc;
	const VectorPath *path = nullptr;

	float quality = 0.5f; // approximation level (more is better)
	Color4F originalColor;

	uint32_t draw(memory::pool_t *pool, const VectorPath &path, const Mat4 &, gl::VertexData *out);
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
	Size targetSize;

	Vector<Pair<Mat4, Rc<gl::VertexData>>> *out = nullptr;

	Data(memory::pool_t *p) : pool(p) {
		transactionPool = memory::pool::create(pool);
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

	void draw(const VectorPath &);
	void draw(const VectorPath &, const Mat4 &);

	void doDraw(const VectorPath &);

	void pushContour(const VectorPath &path, bool closed);
};

static void * staticPoolAlloc(void* userData, unsigned int size) {
	memory::pool_t *pool = (memory::pool_t *)userData;
	size_t s = size;
	return memory::pool::palloc(pool, s);
}

static void staticPoolFree(void * userData, void * ptr) {
	// empty
	TESS_NOTUSED(userData);
	TESS_NOTUSED(ptr);
}

static inline float canvas_approx_err_sq(float e) {
	e = (1.0f / e);
	return e * e;
}

static inline float canvas_dist_sq(float x1, float y1, float x2, float y2) {
	const float dx = x2 - x1, dy = y2 - y1;
	return dx * dx + dy * dy;
}

static void VectorCanvasPathDrawer_setVertexCount(void *ptr, int vertexes, int faces) {
	auto out = (VectorCanvasPathOutput *)ptr;
	out->vertexes->data.resize(vertexes);
	out->vertexes->indexes.reserve(faces * 3);
}

static void VectorCanvasPathDrawer_pushVertex(void *ptr, TESSindex idx, TESSreal x, TESSreal y, TESSreal vertexValue) {
	auto out = (VectorCanvasPathOutput *)ptr;
	if (size_t(idx) >= out->vertexes->data.size()) {
		out->vertexes->data.resize(idx + 1);
	}

	out->vertexes->data[idx] = gl::Vertex_V4F_V4F_T2F2U{
		Vec4(x, y, 0.0f, 1.0f),
		Vec4(out->color.r, out->color.g, out->color.b, out->color.a * vertexValue),
		Vec2(0.0f, 0.0f), 0, 0
	};
}

static void VectorCanvasPathDrawer_pushTriangle(void *ptr, TESSshort pt1, TESSshort pt2, TESSshort pt3) {
	auto out = (VectorCanvasPathOutput *)ptr;
	out->vertexes->indexes.emplace_back(pt1);
	out->vertexes->indexes.emplace_back(pt2);
	out->vertexes->indexes.emplace_back(pt3);
	++ out->objects;
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

Rc<VectorCanvasResult> VectorCanvas::draw(Rc<VectorImageData> &&image, Size targetSize) {
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

	_data->image->draw([&] (const VectorPath &path, const Mat4 &pos) {
		if (pos.isIdentity()) {
			_data->draw(path);
		} else {
			_data->draw(path, pos);
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

void VectorCanvas::Data::draw(const VectorPath &path) {
	bool hasTransform = !path.getTransform().isIdentity();
	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::draw(const VectorPath &path, const Mat4 &mat) {
	auto matTransform = path.getTransform() * mat;
	bool hasTransform = !matTransform.isIdentity();

	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::doDraw(const VectorPath &path) {
	gl::VertexData *outData = nullptr;
	if (out->empty() || !out->back().second->data.empty()) {
		out->emplace_back(transform, Rc<gl::VertexData>::alloc());
	}

	outData = out->back().second.get();
	memory::pool::push(transactionPool);

	do {
		auto ret = pathDrawer.draw(transactionPool, path, transform, outData);
		if (ret == 0) {
			outData->data.clear();
			outData->indexes.clear();
			out->back().first = transform;
		}
	} while (0);

	memory::pool::pop();
	memory::pool::clear(transactionPool);
}

uint32_t VectorCanvasPathDrawer::draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform, gl::VertexData *out) {
	path = &p;

	tessAlloc.memalloc = &staticPoolAlloc;
	tessAlloc.memfree = &staticPoolFree;
	tessAlloc.userData = (void*)pool;

	float pathX = 0.0f;
	float pathY = 0.0f;
	float approxScale = 1.0f;
	auto style = path->getStyle();
	VectorLineDrawer line(pool);

	TESStesselator *fillTess = nullptr;
	if ((style & DrawStyle::Fill) != 0) {
		fillTess = tessNewTess(&tessAlloc);
	}

	// memory::vector<layout::StrokeDrawer *> strokes;

	Vec3 scale; transform.getScale(&scale);
	approxScale = std::max(scale.x, scale.y);
	line.setStyle(style, approxScale * quality, path->getStrokeWidth());

	pathX = 0.0f; pathY = 0.0f;

	auto pushContour = [&] (bool closed) {
		if ((style & DrawStyle::Fill) != 0) {
			size_t count = line.line.size();
			if (closed && count >= 2) {
				// fix tail point error - remove last point if error value is low enough
				const float dist = canvas_dist_sq(line.line[0].x, line.line[0].y, line.line[count - 1].x, line.line[count - 1].y);
				const float err = canvas_approx_err_sq(approxScale * quality);

				if (dist < err) {
					-- count;
				}
			}

			if (fillTess && !line.line.empty() && count > 2) {
				tessAddContour(fillTess, (TESSVec2 *)line.line.data(), int(count));
			}
		}

		if ((style & DrawStyle::Stroke) != 0) {
			/*auto currentStroke = new (pool) layout::StrokeDrawer(path->getStrokeColor(), path->getStrokeWidth(),
					path->getLineJoin(), path->getLineCup(), path->getMiterLimit());
			if (path->isAntialiased()) {
				currentStroke->setAntiAliased(approxScale);
			}
			currentStroke->draw(line.outline, closed);
			strokes.emplace_back(currentStroke);*/
		}
		line.clear();
	};

	auto pathMoveTo = [&] (float x, float y) {
		if (!line.empty()) {
			pushContour(false);
		}

		line.drawLine(x, y);
		pathX = x; pathY = y;
	};

	auto pathLineTo = [&] (float x, float y) {
		line.drawLine(x, y);
		pathX = x; pathY = y;
	};

	auto pathQuadTo = [&] (float x1, float y1, float x2, float y2) {
		line.drawQuadBezier(pathX, pathY, x1, y1, x2, y2);
		pathX = x2; pathY = y2;
	};

	auto pathCubicTo = [&] (float x1, float y1, float x2, float y2, float x3, float y3) {
		line.drawCubicBezier(pathX, pathY, x1, y1, x2, y2, x3, y3);
		pathX = x3; pathY = y3;
	};

	auto pathArcTo = [&] (float rx, float ry, float rot, bool fa, bool fs, float x2, float y2) {
		line.drawArc(pathX, pathY, rx, ry, rot, fa, fs, x2, y2);
		pathX = x2; pathY = y2;
	};

	auto pathClose = [&] () {
		line.drawClose();
		if (!line.empty()) {
			pushContour(true);
		}

		pathX = 0; pathY = 0;
	};

	auto d = path->getPoints().data();
	for (auto &it : path->getCommands()) {
		switch (it) {
		case VectorPath::Command::MoveTo: pathMoveTo(d[0].p.x, d[0].p.y); ++ d; break;
		case VectorPath::Command::LineTo: pathLineTo(d[0].p.x, d[0].p.y); ++ d; break;
		case VectorPath::Command::QuadTo: pathQuadTo(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y); d += 2; break;
		case VectorPath::Command::CubicTo: pathCubicTo(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y, d[2].p.x, d[2].p.y); d += 3; break;
		case VectorPath::Command::ArcTo: pathArcTo(d[0].p.x, d[0].p.y, d[2].f.v, d[2].f.a, d[2].f.b, d[1].p.x, d[1].p.y); d += 3; break;
		case VectorPath::Command::ClosePath: pathClose(); break;
		default: break;
		}
	}

	if (!line.empty()) {
		pushContour(false);
	}

	VectorCanvasPathOutput outPtr;
	outPtr.vertexes = out;
	outPtr.objects = 0;

	if (fillTess) {
		outPtr.color = originalColor * path->getFillColor();

		TESSResultInterface interface;
		interface.target = &outPtr;
		interface.windingRule = TessWindingRule(path->getWindingRule() == Winding::NonZero);

		if (path->isAntialiased() && (path->getStyle() == DrawStyle::Fill || path->getStrokeOpacity() < 96)) {
			interface.antialiasValue = approxScale;
		} else {
			interface.antialiasValue = 0.0f;
		}

		interface.setVertexCount = VectorCanvasPathDrawer_setVertexCount;
		interface.pushVertex = VectorCanvasPathDrawer_pushVertex;
		interface.pushTriangle = VectorCanvasPathDrawer_pushTriangle;

		tessExport(fillTess, &interface);
	}

	return outPtr.objects;
}

SP_EXTERN_C void tessLog(const char *msg) {
	stappler::log::text("XL-Tess", msg);
}

}
