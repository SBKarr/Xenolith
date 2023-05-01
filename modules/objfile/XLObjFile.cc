/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLObjFile.h"

namespace stappler::xenolith::obj {

bool ObjFile::init(FilePath path) {
	auto data = filesystem::readIntoMemory<memory::StandartInterface>(path.get());
	if (!data.empty()) {
		return init(data);
	}
	return false;
}

bool ObjFile::init(BytesView data) {
	return loadFile(StringView((const char *)data.data(), data.size()));
}

bool ObjFile::loadFile(StringView str) {
	auto skipString = [] (StringView &r) {
		do {
			r.skipUntil<StringView::Chars<'\\', '\n'>>();
			if (r.is('\\')) {
				r += 2;
			}
		} while (!r.is('\n'));

		if (r.is('\n')) {
			++ r;
		}
	};

	auto readVertex = [] (StringView &r, geom::Vec4 &target) {
		float *t = &target.x;
		while (!r.empty() && !r.is('\n')) {
			r.skipChars<StringView::Chars<' ', '\t', '\r'>>();
			if (r.is('\\')) {
				r += 2;
			} else {
				auto v = r.readFloat();
				if (t <= &target.w) {
					if (v.grab(*t)) {
						++ t;
					}
				}
			}
			r.skipChars<StringView::Chars<' ', '\t', '\r'>>();
		}

		if (r.is('\n')) {
			++ r;
		}
	};

	auto readString = [] (StringView &r, String &target) {
		while (!r.empty() && !r.is('\n')) {
			r.skipChars<StringView::Chars<' ', '\t', '\r'>>();

			auto v = r.readUntil<StringView::Chars<'\r', '\n', '\\'>>();
			if (!v.empty()) {
				target += v.str<Interface>();
			}
			if (r.is('\r')) {
				++ r;
			}
			if (r.is('\\')) {
				r += 2;
			}
		}

		if (r.is('\n')) {
			++ r;
		}
	};

	auto readFace = [&] (StringView &r, Face &face) {
		while (!r.empty() && !r.is('\n')) {
			r.skipChars<StringView::Chars<' ', '\t', '\r'>>();

			if (r.is('\\')) {
				r += 2;
			} else {
				FaceValue vals;

				auto i = r.readInteger(10).get(0);
				if (i) {
					vals.v = (i > 0) ? i : (_vertexPosition.size() - i + 1);
					if (r.is('/')) {
						++ r;
						i = r.readInteger(10).get(0);
						if (i) {
							vals.vt = (i > 0) ? i : (_vertexTexture.size() - i + 1);
						}
						if (r.is('/')) {
							++ r;
							i = r.readInteger(10).get(0);
							vals.vn = (i > 0) ? i : (_vertexNormal.size() - i + 1);
						}
					}

					face.values.emplace_back(vals);
				}
			}

			r.skipChars<StringView::Chars<' ', '\t', '\r'>>();
		}

		if (r.is('\n')) {
			++ r;
		}
	};

	while (!str.empty()) {
		if (str.is('#')) {
			skipString(str);
		} else if (str.is("vt ")) {
			str += "vt "_len;
			_vertexTexture.emplace_back(geom::Vec4(geom::Vec4::UNIT_W));
			readVertex(str, _vertexTexture.back());
		} else if (str.is("vn ")) {
			str += "vn "_len;
			_vertexNormal.emplace_back(geom::Vec4(geom::Vec4::UNIT_W));
			readVertex(str, _vertexNormal.back());
		} else if (str.is("v ")) {
			str += "v "_len;
			_vertexPosition.emplace_back(geom::Vec4(geom::Vec4::UNIT_W));
			readVertex(str, _vertexPosition.back());
		} else if (str.is("f ")) {
			str += "f "_len;
			_faces.emplace_back(Face());
			readFace(str, _faces.back());
		} else if (str.is("o ")) {
			str += "o "_len;
			_name.clear();
			readString(str, _name);
		} else {
			skipString(str);
		}
	}
	return true;
}

}
