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

#ifndef MODULES_MATERIAL_BASE_MATERIALSURFACEINTERIOR_H_
#define MODULES_MATERIAL_BASE_MATERIALSURFACEINTERIOR_H_

#include "MaterialSurfaceStyle.h"
#include "XLComponent.h"

namespace stappler::xenolith::material {

class SurfaceInterior : public Component {
public:
	static uint64_t ComponentFrameTag;

	virtual ~SurfaceInterior() { }

	virtual bool init() override;
	virtual bool init(SurfaceStyle &&style);

	virtual void onAdded(Node *owner) override;
	virtual void visit(RenderFrameInfo &, NodeFlags parentFlags) override;

	virtual void setStyle(SurfaceStyleData &&style) { _interiorStyle = move(style); }
	virtual const SurfaceStyleData &getStyle() const { return _interiorStyle; }

	virtual bool isOwnedByMaterialNode() const { return _ownerIsMaterialNode; }

protected:
	bool _ownerIsMaterialNode = false;
	SurfaceStyle _assignedStyle;
	SurfaceStyleData _interiorStyle;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALSURFACEINTERIOR_H_ */
