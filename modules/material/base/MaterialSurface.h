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

#ifndef MODULES_MATERIAL_BASE_MATERIALSURFACE_H_
#define MODULES_MATERIAL_BASE_MATERIALSURFACE_H_

#include "MaterialSurfaceStyle.h"
#include "XLVectorSprite.h"

namespace stappler::xenolith::material {

class SurfaceInterior;
class StyleContainer;

class Surface : public VectorSprite {
public:
	static constexpr uint32_t TransitionActionTag = maxOf<uint32_t>() - 1;

	virtual ~Surface() { }

	virtual bool init(const SurfaceStyle &);

	virtual const SurfaceStyle &getStyleOrigin() const { return _styleOrigin; }
	virtual const SurfaceStyle &getStyleTarget() const { return _styleTarget; }

	virtual const SurfaceStyleData &getStyleCurrent() const { return _styleDataCurrent; }

	virtual void setStyle(const SurfaceStyle &);
	virtual void setStyle(const SurfaceStyle &, float duration);

	virtual void setStyleDirtyCallback(Function<void(const SurfaceStyleData &)> &&);
	virtual const Function<void(const SurfaceStyleData &)> &getStyleDirtyCallback() const { return _styleDirtyCallback; }

	virtual bool visitDraw(RenderFrameInfo &, NodeFlags parentFlags) override;

protected:
	virtual void applyStyle(const SurfaceStyleData &);

	virtual StyleContainer *getStyleContainerForFrame(RenderFrameInfo &) const;
	virtual RenderingLevel getRealRenderingLevel() const override;

	virtual void pushShadowCommands(RenderFrameInfo &, NodeFlags flags, const Mat4 &,
			SpanView<gl::TransformedVertexData> = SpanView<gl::TransformedVertexData>()) override;

	SurfaceInterior *_interior = nullptr;

	SurfaceStyle _styleOrigin;
	SurfaceStyle _styleTarget;

	SurfaceStyleData _styleDataOrigin;
	SurfaceStyleData _styleDataTarget;
	SurfaceStyleData _styleDataCurrent;

	Function<void(const SurfaceStyleData &)> _styleDirtyCallback;

	ShapeFamily _realShapeFamily = ShapeFamily::RoundedCorners;
	float _fillValue = 0.0f;
	float _outlineValue = 0.0f;
	float _styleProgress = 0.0f;
	float _realCornerRadius = nan();
	bool _styleDirty = true;
	bool _inTransition = false;
};

class BackgroundSurface : public Surface {
public:
	virtual ~BackgroundSurface() { }

	virtual bool init() override;
	virtual bool init(const SurfaceStyle &) override;

	virtual StyleContainer *getStyleContainer() const { return _styleContainer; }

protected:
	virtual StyleContainer *getStyleContainerForFrame(RenderFrameInfo &) const override;

	StyleContainer *_styleContainer = nullptr;
};

}

#endif /* MODULES_MATERIAL_BASE_MATERIALSURFACE_H_ */
