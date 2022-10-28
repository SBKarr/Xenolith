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

#ifndef XENOLITH_CORE_DIRECTOR_XLTEXTINPUTMANAGER_H_
#define XENOLITH_CORE_DIRECTOR_XLTEXTINPUTMANAGER_H_

#include "XLEventHandler.h"
#include "XLGlView.h"

namespace stappler::xenolith {

class TextInputManager;

// TextInputHandler used to capture text input
// - Only one handler can be active for one view, new running handler will displace old one
// - Owner is responsible for handler's existence and callback's correctness when handler
// is active (attached to manager)
// - setString/setCursor has no meaning if handler is not active
// - Whole input string from handler will be transferred to device input manager, so, try to
// keep it small (e.g. when working with paragraphs, send only current paragraph). Large
// strings can significantly reduce performance
struct TextInputHandler {
	Function<void(const WideStringView &, const TextInputCursor &)> onText;
	Function<void(bool, const Rect &, float)> onKeyboard;
	Function<void(bool)> onInput;
	Function<void()> onEnded;
	//Function<bool(const Vec2 &)> onTouchFilter;
	Rc<TextInputManager> manager;

	~TextInputHandler();

	bool run(TextInputManager *, const WideStringView &str = WideStringView(), const TextInputCursor & = TextInputCursor(),
			TextInputType = TextInputType::Empty);
	void cancel();

	// only if this handler is active
	bool setString(const WideStringView &str, const TextInputCursor & = TextInputCursor());
	bool setCursor(const TextInputCursor &);

	WideStringView getString() const;
	const TextInputCursor &getCursor() const;

	bool isInputEnabled() const;
	bool isKeyboardVisible() const;
	const Rect &getKeyboardRect() const;

	bool isActive() const;
};

class TextInputManager : public Ref {
public:
	TextInputManager();

	bool init(gl::View *);

	bool hasText();
    void insertText(const WideString &sInsert, bool compose = false);
	void textChanged(const WideString &text, const TextInputCursor &);
	void cursorChanged(const TextInputCursor &);
    void deleteBackward();
    void deleteForward();

    // called from device when keyboard is attached to application
    // if keyboard is screen keyboard, it's intersection rect with application window defined in rect, otherwise - Rect::ZERO
    // if screen keyboard state changes is animated, it's time defined in duration, or 0.0f otherwise
	void onKeyboardEnabled(const Rect &rect, float duration);

    // called from device when keyboard is detached from application
	void onKeyboardDisabled(float duration);

	void setInputEnabled(bool enabled);

	void onTextChanged();

	// run input capture (or update it with new params)
	// propagates all data to device input manager, enables screen keyboard if needed
	bool run(TextInputHandler *, const WideStringView &str, const TextInputCursor &, TextInputType type);

	// update current buffer string (and/or internal cursor)
	// propagates string and cursor to device input manager to enable autocorrections, suggestions, etc...
	void setString(const WideStringView &str);
	void setString(const WideStringView &str, const TextInputCursor &);
	void setCursor(const TextInputCursor &);

	WideStringView getString() const;
	const TextInputCursor &getCursor() const;

	// disable text input, disables keyboard connection and keyboard input event interception
	// default manager automatically disabled when app goes background
	void cancel();

	bool isRunning() const { return _running; }
	bool isKeyboardVisible() const { return _isKeyboardVisible; }
	bool isInputEnabled() const { return _isKeyboardVisible; }
	float getKeyboardDuration() const { return _keyboardDuration; }
	const Rect &getKeyboardRect() const { return _keyboardRect; }

	TextInputHandler *getHandler() const { return _handler; }

	bool canHandleInputEvent(const InputEventData &);
	bool handleInputEvent(const InputEventData &);

protected:
	gl::View *_view = nullptr;

	TextInputHandler *_handler = nullptr;
	Rect _keyboardRect;
	float _keyboardDuration = 0.0f;
	bool _isInputEnabled = false;
	bool _isKeyboardVisible = false;
	bool _running = false;

	TextInputType _type = TextInputType::Empty;
	WideString _string;
	TextInputCursor _cursor;
	InputKeyComposeState _compose = InputKeyComposeState::Nothing;
};

}

#endif /* XENOLITH_CORE_DIRECTOR_XLTEXTINPUTMANAGER_H_ */
