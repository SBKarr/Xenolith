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

#ifndef XENOLITH_CORE_XLDEFINE_H_
#define XENOLITH_CORE_XLDEFINE_H_

#include "XLForward.h"
#include "XLProfiling.h"
#include "XLGraphics.h"
#include "XLInput.h"

namespace stappler::xenolith {

static constexpr uint64_t InvalidTag = maxOf<uint64_t>();

constexpr uint64_t operator"" _usec ( unsigned long long int val ) { return val * 1000'000; }
constexpr uint64_t operator"" _umsec ( unsigned long long int val ) { return val * 1000; }
constexpr uint64_t operator"" _umksec ( unsigned long long int val ) { return val; }

class PoolRef : public Ref {
public:
	virtual ~PoolRef() {
		memory::pool::destroy(_pool);
	}
	PoolRef(memory::pool_t *root = nullptr) {
		_pool = memory::pool::create(root);
	}

	memory::pool_t *getPool() const { return _pool; }

	template <typename Callable>
	auto perform(const Callable &cb) {
		memory::pool::context<memory::pool_t *> ctx(_pool);
		return cb();
	}

protected:
	memory::pool_t *_pool = nullptr;
};

struct UpdateTime {
	// global OS timer in microseconds
	uint64_t global;

	// microseconds since application was started
	uint64_t app;

	// microseconds since last update
	uint64_t delta;
};

}

#endif /* XENOLITH_CORE_XLDEFINE_H_ */
