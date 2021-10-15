/**
Copyright (c) 2020-2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_CORE_XLDEFINE_H_
#define COMPONENTS_XENOLITH_CORE_XLDEFINE_H_

#include "XLForward.h"
#include "XLGlEnum.h"

namespace stappler::xenolith {

static constexpr uint64_t InvalidTag = maxOf<uint64_t>();

enum class NodeFlags {
	None,
	TransformDirty = 1 << 0,
	ContentSizeDirty = 1 << 1,

	DirtyMask = TransformDirty | ContentSizeDirty
};

SP_DEFINE_ENUM_AS_MASK(NodeFlags)

namespace AppEvent {
	using Value = uint32_t;

	constexpr uint32_t None = 0;
	constexpr uint32_t Terminate = 1;
	constexpr uint32_t SwapchainRecreation = 2;
	constexpr uint32_t SwapchainRecreationBest = 4;
	constexpr uint32_t Update = 8;
	constexpr uint32_t Thread = 16;
	constexpr uint32_t Input = 32;
}

}

#endif /* COMPONENTS_XENOLITH_CORE_XLDEFINE_H_ */
