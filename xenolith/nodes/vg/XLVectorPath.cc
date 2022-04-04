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

#include "XLVectorPath.h"

namespace stappler::xenolith {

#define SP_PATH_LOG(...)
//#define SP_PATH_LOG(...) stappler::log::format("Path Debug", __VA_ARGS__)

using PathFloat = float;

class SVGPathReader : public ReaderClassBase<char> {
public:
	static bool readFile(VectorPath *p, const StringView &str) {
		if (!str.empty()) {
			auto content = filesystem::readTextFile(str);
			StringView r(content);
			r.skipUntilString("<path ");
			if (!r.is("<path ")) {
				return false;
			}

			r.skipString("<path ");
			StringView pathContent = r.readUntil<Chars<'>'>>();
			pathContent.skipUntilString("d=\"");
			if (r.is("d=\"")) {
				r.skipString("d=\"");
				return readPath(p, pathContent.readUntil<Chars<'"'>>());
			}
		}
		return false;
	}

	static bool readPath(VectorPath *p, const StringView &r) {
		if (r.size() > 0) {
			SVGPathReader reader(p, r);
			return reader.parse();
		}
		return false;
	}

protected:
	bool parse() {
		while (!reader.empty()) {
			if (!readCmdGroup()) {
				return false;
			}
		}

		return true;
	}

	bool readCmdGroup() {
		readWhitespace();
		while (!reader.empty()) {
			if (!readCmd()) {
				return false;
			}
		}
		return true;
	}

	bool readCmd() {
		if (!readMoveTo()) {
			return false;
		}
		readWhitespace();

		bool readNext = true;
		while(readNext) {
			readNext = readDrawTo();
			if (readNext) {
				readWhitespace();
			}
		}

		return true;
	}

	bool readMoveTo() {
		if (reader >= 1) {
			readWhitespace();
			bool relative = true;
			if (reader.is('M')) {
				relative = false; ++ reader;
			} else if (reader.is('m')) {
				relative = true; ++ reader;
			} else {
				return false;
			}

			readWhitespace();
			return readMoveToArgs(relative);
		}
		return false;
	}

	bool readDrawTo() {
		if (reader >= 1) {
			auto c = reader[0];
			++ reader;

			readWhitespace();
			if (c == 'M' || c == 'm') {
				return readMoveToArgs(c == 'm');
			} else if (c == 'Z' || c == 'z') {
				SP_PATH_LOG("Z");
				if (_pathStarted) {
					_x = _sx;
					_y = _sy;
					_pathStarted = false;
				}
				path->closePath();
				return true;
			} else if (c == 'L' || c == 'l') {
				return readLineToArgs(c == 'l');
			} else if (c == 'H' || c == 'h') {
				return readHorizontalLineTo(c == 'h');
			} else if (c == 'V' || c == 'v') {
				return readVerticalLineTo(c == 'v');
			} else if (c == 'C' || c == 'c') {
				return readCubicBezier(c == 'c');
			} else if (c == 'S' || c == 's') {
				return readCubicBezierShort(c == 's');
			} else if (c == 'Q' || c == 'q') {
				return readQuadraticBezier(c == 'q');
			} else if (c == 'T' || c == 't') {
				return readQuadraticBezierShort(c == 't');
			} else if (c == 'A' || c == 'a') {
				return readEllipticalArc(c == 'a');
			}
		}
		return false;
	}

	bool readLineToArgs(bool relative) {
		PathFloat x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readCoordPair(x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) { x = _x + x; y = _y + y; }

				SP_PATH_LOG("L %f %f (%f %f)", x, y, x - _x, y - _y);
				_x = x; _y = y; _b = false;
				path->lineTo(x, y);
			}
		}
		return ret;
	}

	bool readHorizontalLineTo(bool relative) {
		PathFloat x;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readNumber(x);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) { x = _x + x; }

				SP_PATH_LOG("H %f (%f)", x, x - _x);
				_x = x; _b = false;
				path->lineTo(x, _y);
			}
		}
		return ret;
	}

	bool readVerticalLineTo(bool relative) {
		PathFloat y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readNumber(y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) { y = _y + y; }

				SP_PATH_LOG("V %f (%f)", y, y - _y);
				_y = y; _b = false;
				path->lineTo(_x, y);
			}
		}
		return ret;
	}

	bool readCubicBezier(bool relative) {
		PathFloat x1, y1, x2, y2, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readCurveToArg(x1, y1, x2, y2, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) {
					x1 = _x + x1; y1 = _y + y1;
					x2 = _x + x2; y2 = _y + y2;
					x = _x + x; y = _y + y;
				}
				_x = x; _y = y; _bx = x2, _by = y2; _b = true;
				SP_PATH_LOG("C %f %f %f %f %f %f", x1, y1, x2, y2, x, y);
				path->cubicTo(x1, y1, x2, y2, x, y);
			}
		}
		return ret;
	}

	bool readCubicBezierShort(bool relative) {
		PathFloat x1, y1, x2, y2, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readSmoothCurveToArg(x2, y2, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }

				getNewBezierParams(x1, y1);
				if (relative) {
					x2 = _x + x2; y2 = _y + y2;
					x = _x + x; y = _y + y;
				}
				_x = x; _y = y; _bx = x2, _by = y2; _b = true;
				SP_PATH_LOG("S (%f %f) %f %f %f %f", x1, y1, x2, y2, x, y);
				path->cubicTo(x1, y1, x2, y2, x, y);
			}
		}
		return ret;
	}

	bool readQuadraticBezier(bool relative) {
		PathFloat x1, y1, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readQuadraticCurveToArg(x1, y1, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) {
					x1 = _x + x1; y1 = _y + y1;
					x = _x + x; y = _y + y;
				}
				_x = x; _y = y; _bx = x1, _by = y1; _b = true;
				path->quadTo(x1, y1, x, y);
			}
		}
		return ret;
	}

	bool readQuadraticBezierShort(bool relative) {
		PathFloat x1, y1, x, y;
		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readSmoothQuadraticCurveToArg(x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				getNewBezierParams(x1, y1);
				if (relative) {
					x = _x + x; y = _y + y;
				}
				_x = x; _y = y; _bx = x1, _by = y1; _b = true;
				path->quadTo(x1, y1, x, y);
			}
		}
		return ret;
	}

	bool readEllipticalArc(bool relative) {
		PathFloat rx, ry, xAxisRotation, x, y;
		bool largeArc, sweep;

		bool readNext = true, first = true, ret = false;
		while (readNext) {
			readCommaWhitespace();
			readNext = readEllipticalArcArg(rx, ry, xAxisRotation, largeArc, sweep, x, y);
			if (first && !readNext) {
				return false;
			} else if (readNext) {
				if (first) { ret = true; first = false; }
				if (relative) {
					x = _x + x; y = _y + y;
				}

				if (rx == 0 || ry == 0) {
					_x = x; _y = y; _b = false;
					path->lineTo(x, y);
				} else {
					_x = x; _y = y; _b = false;
					path->arcTo(rx, ry, xAxisRotation, largeArc, sweep, x, y);
				}
			}
		}
		return ret;
	}

	bool readMoveToArgs(bool relative) {
		PathFloat x = 0.0f, y = 0.0f;
		if (!readCoordPair(x, y)) {
			return false;
		}

		if (relative) {
			x = _x + x;
			y = _y + y;
		}

		_b = false;
		_x = x;
		_y = y;
		//if (!_pathStarted) {
			_sx = _x;
			_sy = _y;
			_pathStarted = true;
		//}

		SP_PATH_LOG("M %f %f", _x, _y);
		path->moveTo(x, y);
		readCommaWhitespace();
		readLineToArgs(relative);

		return true;
	}

	bool readCurveToArg(PathFloat &x1, PathFloat &y1, PathFloat &x2, PathFloat &y2, PathFloat &x, PathFloat &y) {
		if (!readCoordPair(x1, y1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x2, y2)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}
		return true;
	}

	bool readSmoothCurveToArg(PathFloat &x2, PathFloat &y2, PathFloat &x, PathFloat &y) {
		return readQuadraticCurveToArg(x2, y2, x, y);
	}

	bool readQuadraticCurveToArg(PathFloat &x1, PathFloat &y1, PathFloat &x, PathFloat &y) {
		if (!readCoordPair(x1, y1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}
		return true;
	}

	bool readEllipticalArcArg(PathFloat &_rx, PathFloat &_ry, PathFloat &_xAxisRotation,
			bool &_largeArc, bool &_sweep, PathFloat &_dx, PathFloat &_dy) {
		PathFloat rx, ry, xAxisRotation, x, y;
		bool largeArc, sweep;

		if (!readCoordPair(rx, ry)) {
			return false;
		}
		readCommaWhitespace();
		if (!readNumber(xAxisRotation)) {
			return false;
		}

		if (!readCommaWhitespace()) {
			return false;
		}

		if (!readFlag(largeArc)) {
			return false;
		}

		readCommaWhitespace();
		if (!readFlag(sweep)) {
			return false;
		}

		readCommaWhitespace();
		if (!readCoordPair(x, y)) {
			return false;
		}

		_rx = rx;
		_ry = ry;
		_xAxisRotation = xAxisRotation;
		_largeArc = largeArc;
		_sweep = sweep;
		_dx = x;
		_dy = y;

		return true;
	}

	bool readSmoothQuadraticCurveToArg(PathFloat &x, PathFloat &y) {
		return readCoordPair(x, y);
	}

	bool readCoordPair(PathFloat &x, PathFloat &y) {
		PathFloat value1 = 0.0f, value2 = 0.0f;
		if (!readNumber(value1)) {
			return false;
		}
		readCommaWhitespace();
		if (!readNumber(value2)) {
			return false;
		}

		x = value1;
		y = value2;
		return true;
	}

	bool readWhitespace() { return reader.readChars<Group<GroupId::WhiteSpace>>().size() != 0; }

	bool readCommaWhitespace() {
		if (reader >= 1) {
			bool ws = readWhitespace();
			if (reader.is(',')) {
				++ reader;
			} else {
				return ws;
			}
			readWhitespace();
			return true;
		}
		return false;
	}

	bool readNumber(PathFloat &val) {
		if (!reader.empty()) {
			if (!reader.readFloat().grab(val)) {
				return false;
			}
			return true;
		}
		return false;
	}

	bool readFlag(bool &flag) {
		if (reader >= 1) {
			if (reader.is('0') || reader.is('1')) {
				flag = (reader.is('1'));
				++ reader;
				return true;
			}
		}
		return false;
	}

	void getNewBezierParams(PathFloat &bx, PathFloat &by) {
		if (_b) {
			bx = _x * 2 - _bx; by = _y * 2 - _by;
		} else {
			bx = _x; by = _y;
		}
	}

	SVGPathReader(VectorPath *p, const StringView &r)
	: path(p), reader(r) { }

	PathFloat _x = 0.0f, _y = 0.0f;

	bool _b = false;
	PathFloat _bx = 0.0f, _by = 0.0f;

	PathFloat _sx = 0.0f, _sy = 0.0f;
	bool _pathStarted = false;
	VectorPath *path = nullptr;
	StringView reader;
};

VectorPath::VectorPath() { }
VectorPath::VectorPath(size_t count) {
	_points.reserve(count * 3);
	_commands.reserve(count);
}

VectorPath::VectorPath(const VectorPath &path) : _points(path._points), _commands(path._commands), _params(path._params) { }

VectorPath &VectorPath::operator=(const VectorPath &path) {
	_points = path._points;
	_commands = path._commands;
	_params = path._params;
	return *this;
}

VectorPath::VectorPath(VectorPath &&path) : _points(move(path._points)), _commands(move(path._commands)), _params(move(path._params)) { }

VectorPath &VectorPath::operator=(VectorPath &&path) {
	_points = move(path._points);
	_commands = move(path._commands);
	_params = move(path._params);
	return *this;
}

bool VectorPath::init() {
	return true;
}

bool VectorPath::init(const StringView &str) {
	if (!SVGPathReader::readPath(this, str)) {
		return false;
	}
	return true;
}

bool VectorPath::init(FilePath &&str) {
	if (!SVGPathReader::readFile(this, str.get())) {
		return false;
	}
	return true;
}

bool VectorPath::init(const uint8_t *data, size_t len) {
	float x1, y1, x2, y2, x3, y3;
	uint8_t tmp;

	BytesViewNetwork reader(data, len);

	auto size = data::cbor::_readInt(reader);
	_commands.reserve(size);
	_points.reserve(size * 3);
	for (; size != 0; --size) {
		auto cmd = data::cbor::_readInt(reader);
		switch (uint8_t(cmd)) {
		case toInt(Command::MoveTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			moveTo(x1, y1);
			break;
		case toInt(Command::LineTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			lineTo(x1, y1);
			break;
		case toInt(Command::QuadTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			quadTo(x1, y1, x2, y2);
			break;
		case toInt(Command::CubicTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			x3 = data::cbor::_readNumber(reader);
			y3 = data::cbor::_readNumber(reader);
			cubicTo(x1, y1, x2, y2, x3, y3);
			break;
		case toInt(Command::ArcTo):
			x1 = data::cbor::_readNumber(reader);
			y1 = data::cbor::_readNumber(reader);
			x2 = data::cbor::_readNumber(reader);
			y2 = data::cbor::_readNumber(reader);
			x3 = data::cbor::_readNumber(reader);
			tmp = data::cbor::_readInt(reader);
			arcTo(x1, y1, x3, tmp & 1, tmp & 2, x2, y2);
			break;
		default: break;
		}
	}
	return true;
}

size_t VectorPath::count() const {
	return _commands.size();
}

VectorPath & VectorPath::moveTo(float x, float y) {
	_commands.emplace_back(Command::MoveTo);
	_points.emplace_back(x, y);
	return *this;
}

VectorPath & VectorPath::lineTo(float x, float y) {
	_commands.emplace_back(Command::LineTo);
	_points.emplace_back(x, y);
	return *this;
}

VectorPath & VectorPath::quadTo(float x1, float y1, float x2, float y2) {
	_commands.emplace_back(Command::QuadTo);
	_points.emplace_back(x1, y1);
	_points.emplace_back(x2, y2);
	return *this;
}

VectorPath & VectorPath::cubicTo(float x1, float y1, float x2, float y2, float x3, float y3) {
	_commands.emplace_back(Command::CubicTo);
	_points.emplace_back(x1, y1);
	_points.emplace_back(x2, y2);
	_points.emplace_back(x3, y3);
	return *this;
}

VectorPath & VectorPath::arcTo(float rx, float ry, float angle, bool largeFlag, bool sweepFlag, float x, float y) {
	_commands.emplace_back(Command::ArcTo);
	_points.emplace_back(rx, ry);
	_points.emplace_back(x, y);
	_points.emplace_back(angle, largeFlag, sweepFlag);
	return *this;
}
VectorPath & VectorPath::closePath() {
	_commands.emplace_back(Command::ClosePath);
	return *this;
}

VectorPath & VectorPath::addRect(const Rect& rect) {
	return addRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}
VectorPath & VectorPath::addRect(float x, float y, float width, float height) {
	moveTo(x, y);
	lineTo(x + width, y);
	lineTo(x + width, y + height);
	lineTo(x, y + height);
	closePath();
	return *this;
}
VectorPath & VectorPath::addOval(const Rect& oval) {
	addEllipse(oval.getMidX(), oval.getMidY(), oval.size.width / 2.0f, oval.size.height / 2.0f);
	return *this;
}
VectorPath & VectorPath::addCircle(float x, float y, float radius) {
	moveTo(x + radius, y);
	arcTo(radius, radius, 0, false, false, x, y - radius);
	arcTo(radius, radius, 0, false, false, x - radius, y);
	arcTo(radius, radius, 0, false, false, x, y + radius);
	arcTo(radius, radius, 0, false, false, x + radius, y);
	closePath();
	return *this;
}

VectorPath & VectorPath::addEllipse(float x, float y, float rx, float ry) {
	moveTo(x + rx, y);
	arcTo(rx, ry, 0, false, false, x, y - ry);
	arcTo(rx, ry, 0, false, false, x - rx, y);
	arcTo(rx, ry, 0, false, false, x, y + ry);
	arcTo(rx, ry, 0, false, false, x + rx, y);
	closePath();
	return *this;
}

VectorPath & VectorPath::addArc(const Rect& oval, float startAngle, float sweepAngle) {
	const auto rx = oval.size.width / 2;
	const auto ry = oval.size.height / 2;

	const auto x = rx * cosf(startAngle);
	const auto y = ry * sinf(startAngle);

	const auto sx = rx * cosf(startAngle + sweepAngle);
	const auto sy = ry * sinf(startAngle + sweepAngle);

	moveTo(oval.origin.x + rx + x, oval.origin.y + ry + y);
	arcTo(rx, ry, 0.0f, (sweepAngle > M_PI)?true:false, true, oval.origin.x + rx + sx, oval.origin.y + ry + sy);
	return *this;
}

VectorPath & VectorPath::addRect(float x, float y, float width, float height, float rx, float ry) {
	if (isnan(rx)) {
		rx = 0.0f;
	}
	if (isnan(ry)) {
		ry = 0.0f;
	}

	if (rx == 0.0f && ry == 0.0f) {
		return addRect(x, y, width, height);
	} else if (rx == 0.0f) {
		rx = ry;
	} else if (ry == 0.0f) {
		ry = rx;
	}

	rx = std::min(width / 2.0f, rx);
	ry = std::min(height / 2.0f, ry);

	moveTo(x + width - rx, y);
	arcTo(rx, ry, 0, false, false, x + width, y + ry);
	lineTo(x + width, y + height - ry);
	arcTo(rx, ry, 0, false, false, x + width - rx, y + height);
	lineTo(x + rx, y + height);
	arcTo(rx, ry, 0, false, false, x, y + height - ry);
	lineTo(x, y + ry);
	arcTo(rx, ry, 0, false, false, x + rx, y);
	closePath();

	return *this;
}

VectorPath & VectorPath::addPath(const VectorPath &path) {
	auto &cmds = path.getCommands();
	auto &points = path.getPoints();

	_commands.reserve(_commands.size() + cmds.size());
	for (auto &it : cmds) { _commands.emplace_back(it); }

	_points.reserve(_points.size() + points.size());
	for (auto &it : points) { _points.emplace_back(it); }

	return *this;
}

VectorPath & VectorPath::setFillColor(const Color4B &color) {
	_params.fillColor = color;
	return *this;
}
VectorPath & VectorPath::setFillColor(const Color3B &color, bool preserveOpacity) {
	_params.fillColor = Color4B(color, preserveOpacity?_params.fillColor.a:255);
	return *this;
}
VectorPath & VectorPath::setFillColor(const Color &color, bool preserveOpacity) {
	_params.fillColor = Color4B(color, preserveOpacity?_params.fillColor.a:255);
	return *this;
}
const Color4B &VectorPath::getFillColor() const {
	return _params.fillColor;
}

VectorPath & VectorPath::setStrokeColor(const Color4B &color) {
	_params.strokeColor = color;
	return *this;
}
VectorPath & VectorPath::setStrokeColor(const Color3B &color, bool preserveOpacity) {
	_params.strokeColor = Color4B(color, preserveOpacity?_params.fillColor.a:255);
	return *this;
}
VectorPath & VectorPath::setStrokeColor(const Color &color, bool preserveOpacity) {
	_params.strokeColor = Color4B(color, preserveOpacity?_params.fillColor.a:255);
	return *this;
}
const Color4B &VectorPath::getStrokeColor() const {
	return _params.strokeColor;
}

VectorPath & VectorPath::setFillOpacity(uint8_t value) {
	_params.fillColor.a = value;
	return *this;
}
uint8_t VectorPath::getFillOpacity() const {
	return _params.fillColor.a;
}

VectorPath & VectorPath::setStrokeOpacity(uint8_t value) {
	_params.strokeColor.a = value;
	return *this;
}
uint8_t VectorPath::getStrokeOpacity() const {
	return _params.strokeColor.a;
}

VectorPath & VectorPath::setStrokeWidth(float width) {
	_params.strokeWidth = width;
	return *this;
}

float VectorPath::getStrokeWidth() const {
	return _params.strokeWidth;
}

VectorPath &VectorPath::setWindingRule(Winding value) {
	_params.winding = value;
	return *this;
}
Winding VectorPath::getWindingRule() const {
	return _params.winding;
}

VectorPath &VectorPath::setLineCup(LineCup value) {
	_params.lineCup = value;
	return *this;
}
LineCup VectorPath::getLineCup() const {
	return _params.lineCup;
}

VectorPath &VectorPath::setLineJoin(LineJoin value) {
	_params.lineJoin = value;
	return *this;
}
LineJoin VectorPath::getLineJoin() const {
	return _params.lineJoin;
}

VectorPath &VectorPath::setMiterLimit(float value) {
	_params.miterLimit = value;
	return *this;
}
float VectorPath::getMiterLimit() const {
	return _params.miterLimit;
}

VectorPath & VectorPath::setStyle(DrawStyle s) {
	_params.style = s;
	return *this;
}

DrawStyle VectorPath::getStyle() const {
	return _params.style;
}

VectorPath &VectorPath::setAntialiased(bool val) {
	_params.isAntialiased = val;
	return *this;
}
bool VectorPath::isAntialiased() const {
	return _params.isAntialiased;
}

VectorPath & VectorPath::setTransform(const Mat4 &t) {
	_params.transform = t;
	return *this;
}
VectorPath & VectorPath::applyTransform(const Mat4 &t) {
	_params.transform *= t;
	return *this;
}
const Mat4 &VectorPath::getTransform() const {
	return _params.transform;
}

VectorPath & VectorPath::clear() {
	if (!empty()) {
		_commands.clear();
		_points.clear();
	}
	return *this;
}

VectorPath & VectorPath::setParams(const Params &p) {
	_params = p;
	return *this;
}

VectorPath::Params VectorPath::getParams() const {
	return _params;
}

bool VectorPath::empty() const {
	return _commands.empty();
}

void VectorPath::reserve(size_t s, size_t factor) {
	_commands.reserve(s);
	_points.reserve(s * factor);
}

const Vector<VectorPath::Command> &VectorPath::getCommands() const {
	return _commands;
}

const Vector<VectorPath::CommandData> &VectorPath::getPoints() const {
	return _points;
}


class PathBinaryEncoder {
public: // utility
	PathBinaryEncoder(Bytes *b) : buffer(b) { }

	void emplace(uint8_t c) {
		buffer->emplace_back(c);
	}

	void emplace(const uint8_t *buf, size_t size) {
		size_t tmpSize = buffer->size();
		buffer->resize(tmpSize + size);
		memcpy(buffer->data() + tmpSize, buf, size);
	}

private:
	Bytes *buffer;
};


Bytes VectorPath::encode() const {
	Bytes ret; ret.reserve(_commands.size() * sizeof(Command) + _points.size() * sizeof(CommandData) + 2 * (sizeof(size_t) + 1));
	PathBinaryEncoder enc(&ret);

	data::cbor::_writeInt(enc, _commands.size());
	auto d = _points.data();
	for (auto &it : _commands) {
		data::cbor::_writeInt(enc, toInt(it));
		switch (it) {
		case Command::MoveTo:
		case Command::LineTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			++ d;
			break;
		case Command::QuadTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			d += 2;
			break;
		case Command::CubicTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			data::cbor::_writeNumber(enc, d[2].p.x);
			data::cbor::_writeNumber(enc, d[2].p.y);
			d += 3;
			break;
		case Command::ArcTo:
			data::cbor::_writeNumber(enc, d[0].p.x);
			data::cbor::_writeNumber(enc, d[0].p.y);
			data::cbor::_writeNumber(enc, d[1].p.x);
			data::cbor::_writeNumber(enc, d[1].p.y);
			data::cbor::_writeNumber(enc, d[2].f.v);
			data::cbor::_writeInt(enc, (uint8_t(d[2].f.a) << 1) | uint8_t(d[2].f.b));
			d += 3;
			break;
		default: break;
		}
	}

	return ret;
}

String VectorPath::toString() const {
	StringStream stream;

	auto d = _points.data();
	for (auto &it : _commands) {
		switch (it) {
		case Command::MoveTo:
			stream << "M " << d[0].p.x << "," << d[0].p.y << " ";
			++ d;
			break;
		case Command::LineTo:
			stream << "L " << d[0].p.x << "," << d[0].p.y << " ";
			++ d;
			break;
		case Command::QuadTo:
			stream << "Q " << d[0].p.x << "," << d[0].p.y << " "
					<< d[1].p.x << "," << d[1].p.y << " ";
			d += 2;
			break;
		case Command::CubicTo:
			stream << "C " << d[0].p.x << "," << d[0].p.y << " "
					<< d[1].p.x << "," << d[1].p.y << " "
					<< d[2].p.x << "," << d[2].p.y << " ";
			d += 3;
			break;
		case Command::ArcTo:
			stream << "A " << d[0].p.x << "," << d[0].p.y << " "
					<< d[2].f.v << " " << int(d[2].f.a) << " " << int(d[2].f.b) << " "
					<< d[1].p.x << "," << d[1].p.y << " ";
			d += 3;
			break;
		case Command::ClosePath:
			stream << "Z ";
			break;
		default: break;
		}
	}

	return stream.str();
}

size_t VectorPath::commandsCount() const {
	return _commands.size();
}
size_t VectorPath::dataCount() const {
	return _points.size();
}

}
