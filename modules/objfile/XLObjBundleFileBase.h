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

#ifndef MODULES_OBJFILE_XLOBJBUNDLEFILEBASE_H_
#define MODULES_OBJFILE_XLOBJBUNDLEFILEBASE_H_

#include "SPMemory.h"
#include "SPRef.h"
#include "SPFilesystem.h"
#include "SPVec4.h"

namespace stappler::xenolith::obj {

class ObjFile;

class ObjBundleFileBase : public RefBase<memory::StandartInterface> {
public:
	static constexpr auto Signature = "xobjver1";

	using Interface = memory::StandartInterface;

	template <typename T>
	using Vector = typename Interface::VectorType<T>;

	using String = typename Interface::StringType;
	using Bytes = typename Interface::BytesType;

	enum OpenMode {
		Read,
		Write,
		ReadWrite,
	};

	enum class BlockElementType : uint8_t {
		Object,
		String,
		Vertex,
		Index,
		UserData,
	};

	enum class BlockFlags : uint8_t {
		None = 0,
		Compressed = 1 << 0,
	};

	struct FileHeader {
		uint8_t header[8];
		uint16_t nblocks;
		uint16_t reserved;
		uint64_t fileSize;
	};

	struct BlockHeader {
		BlockElementType type;
		BlockFlags flags;
		uint16_t eltSize;
		uint32_t eltCount;
		uint64_t fileOffset;
		uint64_t fileSize;
	};

	struct FileStruct {
		FileHeader header;
		BlockHeader blocks[4];
	};

	struct Vertex {
		geom::Vec4 pos;
		geom::Vec4 norm;
		geom::Vec2 tex;
		uint32_t user1 = 0;
		uint32_t user2 = 0;

		bool operator==(const Vertex &) const = default;
		bool operator!=(const Vertex &) const = default;
	};

	using Index = uint32_t;

	struct Object {
		uint32_t indexOffset;
		uint32_t indexSize;
		uint32_t nameOffset;
		uint32_t nameSize;
	};

	virtual ~ObjBundleFileBase();

	virtual bool init(OpenMode = Write);
	virtual bool init(FilePath, OpenMode = Read);
	virtual bool init(BytesView, OpenMode = Read);

	virtual void addObject(const ObjFile &);
	virtual void addObject(const ObjFile &, StringView);

	virtual bool save(FilePath, BlockFlags = BlockFlags::None) const;
	virtual Interface::BytesType save(BlockFlags = BlockFlags::None) const;

	SpanView<Object> getObjects() const { return _objects; }
	StringView getObjectName(const Object &obj) const { return StringView(_strings.data() + obj.nameOffset, obj.nameSize); }

protected:
	bool readFile(FilePath, bool weak);
	bool readFile(BytesView);
	bool readStruct(const Callback<bool(uint8_t *buf, size_t bytesCount)> &readCallback);

	void setup(FileStruct &) const;

	const Vertex *findVertex(const Vertex &) const;

	OpenMode _mode = OpenMode::Read;

	Vector<Vertex> _vertexes;
	Vector<Index> _indexes;
	Vector<Object> _objects;
	Vector<char> _strings;

	FileStruct _fileStruct;
	String _filePath;
};

SP_DEFINE_ENUM_AS_MASK(ObjBundleFileBase::BlockFlags)

}

#endif /* MODULES_OBJFILE_XLOBJBUNDLEFILEBASE_H_ */
