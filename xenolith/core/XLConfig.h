/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_CORE_XLCONFIG_H_
#define XENOLITH_CORE_XLCONFIG_H_

#include "SPCommon.h"
#include <stdint.h>

namespace stappler::xenolith::config {

/* Number of child slot, that will be preallocated on first child addition (not on node creation!) */
static constexpr size_t NodePreallocateChilds = 4;

/* Presentation Scheduler interval, used for non-blocking vkWaitForFence */
static constexpr uint64_t PresentationSchedulerInterval = 500; // 500 ms or 1/32 of 60fps frame

/* Minimal safe interval offset for frame timeout scheduling, to ensure, that actual timeout is less then nominal */
static constexpr uint64_t FrameIntervalSafeOffset = 200;

/* Max sampled image descriptors per material texture set (can be actually lower due maxPerStageDescriptorSampledImages) */
static constexpr uint32_t MaxTextureSetImages = 1024;

/* Number of frames, that can be performed in suboptimal swapchain modes */
static constexpr uint32_t MaxSuboptimalFrame = 24;

/* Maximum images in single material */
static constexpr size_t MaxMaterialImages = 4;

static constexpr uint32_t MaxAmbientLights = 16;
static constexpr uint32_t MaxDirectLights = 16;

#if DEBUG
static constexpr uint64_t MaxDirectorDeltaTime = 10'000'000 / 16;
#else
static constexpr uint64_t MaxDirectorDeltaTime = 100'000'000 / 16;
#endif

// max chars count, used by locale::hasLocaleTagsFast
static constexpr size_t MaxFastLocaleChars = size_t(127);

// offset for vertex-based antialiasing in vector images
static constexpr float VGAntialiasFactor = 0.5f;

static constexpr bool VGProcessIntersectsInDrawer = false;

inline uint16_t getGlThreadCount() {
#if DEBUG
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(2), uint16_t(4));
#else
	return math::clamp(uint16_t(std::thread::hardware_concurrency()), uint16_t(4), uint16_t(16));
#endif
}

inline uint16_t getMainThreadCount() {
#if DEBUG
	return math::clamp(uint16_t(std::thread::hardware_concurrency() / 2), uint16_t(2), uint16_t(4));
#else
	return math::clamp(uint16_t(std::thread::hardware_concurrency() / 2), uint16_t(2), uint16_t(16));
#endif
}

}

#endif /* XENOLITH_CORE_XLCONFIG_H_ */
