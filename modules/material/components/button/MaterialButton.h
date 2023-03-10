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

#ifndef MODULES_MATERIAL_COMPONENTS_BUTTON_MATERIALBUTTON_H_
#define MODULES_MATERIAL_COMPONENTS_BUTTON_MATERIALBUTTON_H_

#include "MaterialSurface.h"
#include "MaterialIconSprite.h"
#include "MaterialMenuSource.h"
#include "XLSubscriptionListener.h"

namespace stappler::xenolith::material {

class TypescaleLabel;
class MenuSource;
class MenuSourceButton;

class Button : public Surface {
public:
	static constexpr TimeInterval LongPressInterval = TimeInterval::milliseconds(350);

	virtual ~Button() { }

	virtual bool init(NodeStyle = NodeStyle::Filled,
			ColorRole = ColorRole::Primary, uint32_t schemeTag = SurfaceStyle::PrimarySchemeTag);
	virtual bool init(const SurfaceStyle &) override;

	virtual void onContentSizeDirty() override;

	virtual void setFollowContentSize(bool);
	virtual bool isFollowContentSize() const;

	virtual void setSwallowEvents(bool);
	virtual bool isSwallowEvents() const;

	virtual void setEnabled(bool);
	virtual bool isEnabled() const { return _enabled; }
	virtual bool isMenuSourceButtonEnabled() const;

	virtual void setText(StringView);
	virtual StringView getText() const;

	virtual void setIconSize(float);
	virtual float getIconSize() const;

	virtual void setLeadingIconName(IconName);
	virtual IconName getLeadingIconName() const;

	virtual void setTrailingIconName(IconName);
	virtual IconName getTrailingIconName() const;

	virtual void setTapCallback(Function<void()> &&);
	virtual void setLongPressCallback(Function<void()> &&);
	virtual void setDoubleTapCallback(Function<void()> &&);

	virtual void setMenuSourceButton(Rc<MenuSourceButton> &&);

protected:
	virtual void updateSizeFromContent();
	virtual void updateActivityState();

	virtual void handleTap();
	virtual void handleLongPress();
	virtual void handleDoubleTap();

	virtual float getWidthForContent() const;

	virtual void updateMenuButtonSource();

	InputListener *_inputListener = nullptr;
	TypescaleLabel *_label = nullptr;
	IconSprite *_leadingIcon = nullptr;
	IconSprite *_trailingIcon = nullptr;

	Rc<MenuSource> _floatingMenuSource;
	DataListener<MenuSourceButton> *_menuButtonListener = nullptr;

	Function<void()> _callbackTap;
	Function<void()> _callbackLongPress;
	Function<void()> _callbackDoubleTap;
	float _activityAnimationDuration = 0.25f;

	bool _followContentSize = true;
	bool _mouseOver = false;
	bool _enabled = true;
	bool _focused = false;
	bool _pressed = false;
	bool _selected = false;
	bool _longPressInit = false;
};

}

#endif /* MODULES_MATERIAL_COMPONENTS_BUTTON_MATERIALBUTTON_H_ */
