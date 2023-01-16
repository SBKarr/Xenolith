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

#ifndef MODULES_MATERIAL_COMPONENTS_MATERIALBUTTON_H_
#define MODULES_MATERIAL_COMPONENTS_MATERIALBUTTON_H_

#include "MaterialSurface.h"
#include "XLIconNames.h"

namespace stappler::xenolith::material {

class TypescaleLabel;

struct ButtonData {
	String text;
	IconName iconPrefix = IconName::None;
	IconName iconPostfix = IconName::None;
	Function<void()> callbackTap;
	Function<void()> callbackLongPress;
	Function<void()> callbackDoubleTap;
	bool followContentSize = true;
	float activityAnimationDuration = 0.25f;
};

class Button : public Surface {
public:
	static constexpr TimeInterval LongPressInterval = TimeInterval::milliseconds(350);

	virtual ~Button() { }

	virtual bool init(ButtonData &&, NodeStyle = NodeStyle::Filled,
			ColorRole = ColorRole::Primary, uint32_t schemeTag = SurfaceStyle::PrimarySchemeTag);
	virtual bool init(ButtonData &&, const SurfaceStyle &);

	virtual void onContentSizeDirty() override;

	virtual void setFollowContentSize(bool);
	virtual bool isFollowContentSize() const;

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }

protected:
	virtual void updateButtonData();
	virtual void updateSizeFromContent();
	virtual void updateActivityState();

	virtual void handleTap();
	virtual void handleLongPress();
	virtual void handleDoubleTap();

	InputListener *_inputListener = nullptr;
	TypescaleLabel *_label = nullptr;
	VectorSprite *_iconPrefix = nullptr;
	VectorSprite *_iconPostfix = nullptr;

	ButtonData _buttonData;
	bool _mouseOver = false;
	bool _enabled = true;
	bool _focused = false;
	bool _pressed = false;
	bool _longPressInit = false;
};

}

#endif /* MODULES_MATERIAL_COMPONENTS_MATERIALBUTTON_H_ */
