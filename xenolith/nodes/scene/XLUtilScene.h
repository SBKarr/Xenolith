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

#ifndef XENOLITH_NODES_SCENE_XLUTILSCENE_H_
#define XENOLITH_NODES_SCENE_XLUTILSCENE_H_

#include "XLScene.h"

namespace stappler::xenolith {

class VectorSprite;

class UtilScene : public Scene {
public:
	class FpsDisplay;

	virtual ~UtilScene() { }

	virtual bool init(Application *, RenderQueue::Builder &&, const gl::FrameContraints &) override;

	virtual void update(const UpdateTime &time) override;

	virtual void onContentSizeDirty() override;

	virtual void setFpsVisible(bool);
	virtual bool isFpsVisible() const;

protected:
	void initialize(Application *app);

	void updateInputEventData(InputEventData &data, const InputEventData &source, Vec2 pos, uint32_t id);

	InputEventData _data1 = InputEventData{maxOf<uint32_t>() - 1};
	InputEventData _data2 = InputEventData{maxOf<uint32_t>() - 2};
	InputListener *_listener = nullptr;
	FpsDisplay *_fps = nullptr;
	VectorSprite *_pointerReal = nullptr;
	VectorSprite *_pointerVirtual = nullptr;
	VectorSprite *_pointerCenter = nullptr;
};

}

#endif /* XENOLITH_NODES_SCENE_XLUTILSCENE_H_ */
