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

#include "MaterialSurfaceInterior.h"

#include "MaterialStyleContainer.h"
#include "MaterialSurface.h"
#include "MaterialLayerSurface.h"
#include "XLRenderFrameInfo.h"

namespace stappler::xenolith::material {

uint64_t SurfaceInterior::ComponentFrameTag = Component::GetNextComponentId();

bool SurfaceInterior::init() {
	if (!Component::init()) {
		return false;
	}

	_frameTag = ComponentFrameTag;
	return true;
}

bool SurfaceInterior::init(SurfaceStyle &&style) {
	if (!Component::init()) {
		return false;
	}

	_frameTag = ComponentFrameTag;
	_assignedStyle = move(style);
	return true;
}

void SurfaceInterior::onAdded(Node *owner) {
	Component::onAdded(owner);

	_ownerIsMaterialNode = ((dynamic_cast<Surface *>(_owner) != nullptr) || (dynamic_cast<LayerSurface *>(_owner) != nullptr));
}

void SurfaceInterior::visit(RenderFrameInfo &info, NodeFlags parentFlags) {
	Component::visit(info, parentFlags);

	if (!_ownerIsMaterialNode) {
		auto style = info.getComponent<StyleContainer>(StyleContainer::ComponentFrameTag);
		if (!style) {
			return;
		}

		_assignedStyle.apply(_interiorStyle, _owner->getContentSize(), style);
	}
}

}
