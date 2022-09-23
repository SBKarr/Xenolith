/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef TEST_SRC_ROOT_APPROOTLAYOUT_H_
#define TEST_SRC_ROOT_APPROOTLAYOUT_H_

#include "XLLayer.h"
#include "XLLabel.h"
#include "XLVectorSprite.h"
#include "XLIconNames.h"

#include "AppSlider.h"
#include "AppCheckbox.h"

namespace stappler::xenolith::app {

class RootLayout : public Node {
public:
	virtual ~RootLayout() { }

	virtual bool init() override;

	virtual void onContentSizeDirty() override;

	virtual void update(const UpdateTime &) override;

protected:
	void updateIcon(IconName);
	void updateQualityValue(float);
	void updateScaleValue(float);
	void updateAntialiasValue(bool);

	// Action_gavel_solid
	// IconName::Av_queue_music_outline
	IconName _currentName = IconName::Action_text_rotate_vertical_solid;

	Label *_label = nullptr;
	Label *_info = nullptr;
	Layer *_spriteLayer = nullptr;
	VectorSprite *_sprite = nullptr;
	VectorSprite *_triangles = nullptr;

	Label *_qualityLabel = nullptr;
	AppSlider *_qualitySlider = nullptr;

	Label *_scaleLabel = nullptr;
	AppSlider *_scaleSlider = nullptr;

	Label *_visibleLabel = nullptr;
	AppCheckbox *_visibleCheckbox = nullptr;

	Label *_antialiasLabel = nullptr;
	AppCheckbox *_antialiasCheckbox = nullptr;

	bool _antialias = false;
};

}

#endif /* TEST_SRC_ROOT_APPROOTLAYOUT_H_ */
