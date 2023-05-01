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

#ifndef MODULES_OBJFILE_XLOBJFILE_H_
#define MODULES_OBJFILE_XLOBJFILE_H_

#include "SPMemory.h"
#include "SPRef.h"
#include "SPFilesystem.h"
#include "SPVec4.h"

namespace stappler::xenolith::obj {

class ObjFile : public RefBase<memory::StandartInterface> {
public:
	using Interface = memory::StandartInterface;

	template <typename T>
	using Vector = typename Interface::VectorType<T>;

	using String = typename Interface::StringType;

	struct FaceValue {
		uint32_t v = 0;
		uint32_t vt = 0;
		uint32_t vn = 0;
	};

	struct Face {
		Vector<FaceValue> values;
	};

	virtual ~ObjFile() { }

	bool init(FilePath);
	bool init(BytesView);

	StringView getName() const { return _name; }

	SpanView<Face> getFaces() const { return _faces; }

	const geom::Vec4 *getPosition(uint32_t i) const { return i > 0 && i <= _vertexPosition.size() ? &_vertexPosition[i - 1] : nullptr; }
	const geom::Vec4 *getTexture(uint32_t i) const { return i > 0 && i <= _vertexTexture.size() ? &_vertexTexture[i - 1] : nullptr; }
	const geom::Vec4 *getNormal(uint32_t i) const { return i > 0 && i <= _vertexNormal.size() ? &_vertexNormal[i - 1] : nullptr; }

	const geom::Vec4 *getPosition(const FaceValue &f) const { return getPosition(f.v); }
	const geom::Vec4 *getTexture(const FaceValue &f) const { return getTexture(f.vt); }
	const geom::Vec4 *getNormal(const FaceValue &f) const { return getNormal(f.vn); }

protected:
	bool loadFile(StringView);

	Vector<geom::Vec4> _vertexPosition;
	Vector<geom::Vec4> _vertexTexture;
	Vector<geom::Vec4> _vertexNormal;
	Vector<Face> _faces;
	String _name;
};

}

#endif /* MODULES_OBJFILE_XLOBJFILE_H_ */
