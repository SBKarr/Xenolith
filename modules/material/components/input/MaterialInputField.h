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

#ifndef MODULES_MATERIAL_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_
#define MODULES_MATERIAL_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_

#include "MaterialSurface.h"
#include "MaterialInputTextContainer.h"
#include "MaterialIconSprite.h"

namespace stappler::xenolith::material {

enum class InputFieldStyle {
	Filled,
	Outlined,
};

enum class InputFieldPasswordMode {
	NotPassword,
	ShowAll,
	ShowChar,
	ShowNone,
};

enum class InputFieldError {
	None,
	Overflow,
	InvalidChar,
};

class InputField : public Surface {
public:
	static constexpr uint32_t InputEnabledActionTag = maxOf<uint32_t>() - 2;
	static constexpr uint32_t InputEnabledLabelActionTag = maxOf<uint32_t>() - 3;

	virtual ~InputField();

	virtual bool init(InputFieldStyle = InputFieldStyle::Filled);
	virtual bool init(InputFieldStyle, const SurfaceStyle &);

	virtual void onContentSizeDirty() override;

	virtual void setLabelText(StringView);
	virtual StringView getLabelText() const;

	virtual void setSupportingText(StringView);
	virtual StringView getSupportingText() const;

	virtual void setLeadingIconName(IconName);
	virtual IconName getLeadingIconName() const;

	virtual void setTrailingIconName(IconName);
	virtual IconName getTrailingIconName() const;

	virtual WideStringView getInputString() const { return _inputString; }

protected:
	virtual void updateActivityState();
	virtual void updateInputEnabled();

	virtual void acquireInput(const Vec2 &targetLocation);

	virtual void handleTextInput(WideStringView, TextCursor, TextCursor);
	virtual void handleKeyboardEnabled(bool, const Rect &, float);
	virtual void handleInputEnabled(bool);

	virtual bool handleInputChar(char16_t);

	virtual void handleError(InputFieldError);

	InputFieldStyle _style = InputFieldStyle::Filled;
	InputListener *_inputListener = nullptr;
	InputListener *_focusInputListener = nullptr;
	TypescaleLabel *_labelText = nullptr;
	TypescaleLabel *_supportingText = nullptr;
	InputTextContainer *_container = nullptr;
	IconSprite *_leadingIcon = nullptr;
	IconSprite *_trailingIcon = nullptr;
	Surface *_indicator = nullptr;

	WideString _inputString;
	TextInputHandler _handler;
	TextCursor _cursor;
	TextCursor _markedRegion = TextCursor::InvalidCursor;
	TextInputType _inputType = TextInputType::Default;
	InputFieldPasswordMode _passwordMode = InputFieldPasswordMode::NotPassword;

	float _activityAnimationDuration = 0.25f;
	bool _mouseOver = false;
	bool _enabled = true;
	bool _focused = false;
};

}

#endif /* MODULES_MATERIAL_COMPONENTS_INPUT_MATERIALINPUTFIELD_H_ */
