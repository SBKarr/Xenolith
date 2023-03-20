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

#include "MaterialStyleMonitor.h"
#include "MaterialSurfaceInterior.h"
#include "XLRenderFrameInfo.h"

namespace stappler::xenolith::material {

StyleMonitor::~StyleMonitor() { }

bool StyleMonitor::init(StyleCallback &&cb) {
	if (!Component::init()) {
		return false;
	}

	_styleCallback = move(cb);

	return true;
}

void StyleMonitor::setStyleCallback(StyleCallback &&cb) {
	_styleCallback = move(cb);
}

const StyleMonitor::StyleCallback &StyleMonitor::getStyleCallback() const {
	return _styleCallback;
}

void StyleMonitor::setDirty(bool value) {
	_dirty = value;
}

void StyleMonitor::visit(RenderFrameInfo &frame, NodeFlags parentFlags) {
	auto container = frame.getComponent<StyleContainer>(SurfaceInterior::ComponentFrameTag);
	auto style = frame.getComponent<SurfaceInterior>(SurfaceInterior::ComponentFrameTag);
	if (_dirty || style->getStyle() != _interiorData) {
		_interiorData = style->getStyle();

		auto scheme = container->getScheme(_interiorData.schemeTag);
		_styleCallback(scheme, _interiorData);
		_dirty = false;
	}

	Component::visit(frame, parentFlags);
}

}
