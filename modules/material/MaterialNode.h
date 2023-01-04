/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef MODULES_MATERIAL_MATERIALNODE_H_
#define MODULES_MATERIAL_MATERIALNODE_H_

#include "style/MaterialStyleData.h"
#include "XLVectorSprite.h"

namespace stappler::xenolith::material {

class MaterialNodeInterior : public Component {
public:
	static uint64_t ComponentFrameTag;

	virtual ~MaterialNodeInterior() { }

	virtual bool init() override;
	virtual bool init(StyleData &&style);

	virtual void onAdded(Node *owner) override;
	virtual void visit(RenderFrameInfo &, NodeFlags parentFlags) override;

	virtual void setStyle(StyleData &&style) { _interiorStyle = move(style); }
	virtual const StyleData &getStyle() const { return _interiorStyle; }

	virtual bool isOwnedByMaterialNode() const { return _ownerIsMaterialNode; }

protected:
	bool _ownerIsMaterialNode = false;
	StyleData _interiorStyle;
};

class MaterialNode : public VectorSprite {
public:
	static constexpr uint32_t TransitionActionTag = maxOf<uint32_t>() - 1;

	virtual ~MaterialNode() { }

	virtual bool init(StyleData &&);
	virtual bool init(const StyleData &);

	virtual const StyleData &getStyleOrigin() const { return _styleOrigin; }
	virtual const StyleData &getStyleTarget() const { return _styleTarget; }
	virtual const StyleData &getStyleCurrent() const { return _styleCurrent; }

	virtual void setStyle(StyleData &&);
	virtual void setStyle(StyleData &&, float duration);

	virtual bool visitDraw(RenderFrameInfo &, NodeFlags parentFlags) override;

protected:
	virtual void applyStyle(const StyleData &);

	MaterialNodeInterior *_interior = nullptr;

	StyleData _styleOrigin;
	StyleData _styleTarget;
	StyleData _styleCurrent;
	float _styleProgress = 0.0f;
	float _realCornerRadius = nan();
	bool _styleDirty = true;
	bool _inTransition = false;
};

}

#endif /* MODULES_MATERIAL_MATERIALNODE_H_ */
