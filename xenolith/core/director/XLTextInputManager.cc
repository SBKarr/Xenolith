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

#include "XLTextInputManager.h"

namespace stappler::xenolith {

TextInputHandler::~TextInputHandler() {
	cancel();
}

bool TextInputHandler::run(TextInputManager *manager, WideStringView str, TextCursor cursor, TextCursor marked, TextInputType type) {
	if (!isActive()) {
		this->manager = manager;
		return manager->run(this, str, cursor, marked, type);
	}
	return false;
}

void TextInputHandler::cancel() {
	if (isActive()) {
		this->manager->cancel();
		this->manager = nullptr;
	}
}

// only if this handler is active
bool TextInputHandler::setString(WideStringView str, TextCursor c, TextCursor m) {
	if (isActive()) {
		this->manager->setString(str, c, m);
		return true;
	}
	return false;
}

bool TextInputHandler::setCursor(TextCursor c) {
	if (isActive()) {
		this->manager->setCursor(c);
		return true;
	}
	return false;
}

bool TextInputHandler::setMarked(TextCursor c) {
	if (isActive()) {
		this->manager->setMarked(c);
		return true;
	}
	return false;
}

WideStringView TextInputHandler::getString() const {
	return this->manager->getString();
}
TextCursor TextInputHandler::getCursor() const {
	return this->manager->getCursor();
}
TextCursor TextInputHandler::getMarked() const {
	return this->manager->getMarked();
}

bool TextInputHandler::isInputEnabled() const {
	return this->manager->isInputEnabled();
}
bool TextInputHandler::isKeyboardVisible() const {
	return this->manager->isKeyboardVisible();
}
const Rect &TextInputHandler::getKeyboardRect() const {
	return this->manager->getKeyboardRect();
}

bool TextInputHandler::isActive() const {
	return this->manager && this->manager->getHandler() == this;
}

TextInputManager::TextInputManager() { }

bool TextInputManager::init(TextInputViewInterface *view) {
	_view = view;
	return true;
}

bool TextInputManager::hasText() {
	return _string.length() != 0;
}

void TextInputManager::insertText(WideStringView sInsert, bool compose) {
	if (doInsertText(sInsert, compose)) {
		onTextChanged();
	}
}

void TextInputManager::insertText(WideStringView sInsert, TextCursor replacement) {
	if (replacement.start != maxOf<uint32_t>()) {
		_cursor = replacement;
	}

	if (doInsertText(sInsert, false)) {
		onTextChanged();
	}
}

void TextInputManager::setMarkedText(WideStringView sInsert, TextCursor replacement, TextCursor marked) {
	if (replacement.start != maxOf<uint32_t>()) {
		_cursor = replacement;
	}

	auto start = _cursor.start;

	if (doInsertText(sInsert, false)) {
		_marked = TextCursor(start + marked.start, marked.length);
		onTextChanged();
	}
}

void TextInputManager::textChanged(WideStringView text, TextCursor cursor, TextCursor marked) {
	if (text.size() == 0) {
		_string = WideString();
		_cursor.start = 0;
		_cursor.length = 0;
		_marked = TextCursor::InvalidCursor;
		onTextChanged();
		return;
	}

	_string = text.str<Interface>();
	_cursor = cursor;
	_marked = marked;
	onTextChanged();
}

void TextInputManager::cursorChanged(TextCursor cursor) {
	_cursor = cursor;
	onTextChanged();
}

void TextInputManager::markedChanged(TextCursor marked) {
	_marked = marked;
	onTextChanged();
}

void TextInputManager::deleteBackward() {
	if (_string.empty()) {
		return;
	}

	if (_cursor.length > 0) {
		_string.erase(_string.begin() + _cursor.start, _string.begin() + _cursor.start + _cursor.length);
		_cursor.length = 0;
		onTextChanged();
		return;
	}

	if (_cursor.start == 0) {
		return;
	}

	// TODO: check for surrogate pairs
	size_t nDeleteLen = 1;

	if (_string.length() <= nDeleteLen) {
		_string = WideString();
		_cursor.start = 0;
		_cursor.length = 0;
		onTextChanged();
		return;
	}

	_string.erase(_string.begin() + _cursor.start - 1, _string.begin() + _cursor.start - 1 + nDeleteLen);
	_cursor.start -= nDeleteLen;
	onTextChanged();
}

void TextInputManager::deleteForward() {
	if (_string.empty()) {
		return;
	}

	if (_cursor.length > 0) {
		_string.erase(_string.begin() + _cursor.start, _string.begin() + _cursor.start + _cursor.length);
		_cursor.length = 0;
		onTextChanged();
		return;
	}

	if (_cursor.start == _string.size()) {
		return;
	}

	// TODO: check for surrogate pairs
	size_t nDeleteLen = 1;

	if (_string.length() <= nDeleteLen) {
		_string = WideString();
		_cursor.start = 0;
		_cursor.length = 0;
		onTextChanged();
		return;
	}

	_string.erase(_string.begin() + _cursor.start, _string.begin() + _cursor.start + nDeleteLen);
	onTextChanged();
}

void TextInputManager::unmarkText() {
	markedChanged(TextCursor::InvalidCursor);
}

void TextInputManager::onKeyboardEnabled(const Rect &rect, float duration) {
	_keyboardRect = rect;
	_keyboardDuration = duration;
	if (!_keyboardRect.equals(Rect::ZERO)) {
		_isKeyboardVisible = true;
		if (_handler) {
			_handler->onKeyboard(true, rect, duration);
		}
	}
}

void TextInputManager::onKeyboardDisabled(float duration) {
	_keyboardDuration = duration;
	if (!_keyboardRect.equals(Rect::ZERO)) {
		_isKeyboardVisible = false;
		if (_handler && _handler->onKeyboard) {
			_handler->onKeyboard(false, Rect::ZERO, duration);
		}
	}
	_keyboardRect = Rect::ZERO;
}

void TextInputManager::setInputEnabled(bool enabled) {
	if (_isInputEnabled != enabled) {
		_isInputEnabled = enabled;
		_compose = InputKeyComposeState::Nothing;
		if (_handler && _handler->onInput) {
			_handler->onInput(enabled);
		}
		if (!_isInputEnabled) {
			cancel();
		}
	}
}

void TextInputManager::onTextChanged() {
	if (_handler && _handler->onText) {
		_handler->onText(_string, _cursor, _marked);
	}
}

bool TextInputManager::run(TextInputHandler *h, WideStringView str, TextCursor cursor, TextCursor marked, TextInputType type) {
	auto oldH = _handler;
	_handler = h;
	if (oldH && _running) {
		oldH->onInput(false);
	}
	_cursor = cursor;
	_marked = marked;
	_string = str.str<Interface>();
	_type = type;
	_handler = h;
	if (cursor.start > str.size()) {
		_cursor.start = (uint32_t)str.size();
	}
	if (!_running) {
		_view->runTextInput(_string, cursor.start, cursor.length, type);
		_running = true;
	} else {
		_view->updateTextInput(_string, cursor.start, cursor.length, type);
		if (_running) {
			_handler->onInput(true);
		}
		return false;
	}
	_compose = InputKeyComposeState::Nothing;
	return true;
}

void TextInputManager::setString(WideStringView str, TextCursor cursor, TextCursor marked) {
	_cursor = cursor;
	_marked = marked;
	_string = str.str<Interface>();
	if (cursor.start > str.size()) {
		_cursor.start = (uint32_t)str.size();
	}

	_view->updateTextInput(_string, cursor.start, cursor.length, _type);
}

void TextInputManager::setCursor(TextCursor cursor) {
	_cursor = cursor;
	if (cursor.start > _string.length()) {
		_cursor.start = (uint32_t)_string.length();
	}

	if (_running) {
		_view->updateTextCursor(_cursor.start, _cursor.length);
	}
}

void TextInputManager::setMarked(TextCursor marked) {
	_marked = marked;
	if (marked.start > _string.length()) {
		_marked.start = (uint32_t)_string.length();
	}
}

WideStringView TextInputManager::getString() const {
	return _string;
}

WideStringView TextInputManager::getStringByRange(TextCursor cursor) {
	WideStringView str = _string;
	if (cursor.start >= str.size()) {
		return WideStringView();
	}

	str += cursor.start;
	if (cursor.length >= str.size()) {
		return str;
	}

	return str.sub(0, cursor.length);
}
TextCursor TextInputManager::getCursor() const {
	return _cursor;
}
TextCursor TextInputManager::getMarked() const {
	return _marked;
}

void TextInputManager::cancel() {
	if (_running) {
		_view->cancelTextInput();
		setInputEnabled(false);
		_handler = nullptr;

		_string = WideString();
		_cursor.start = 0;
		_cursor.length = 0;
		_running = false;
	}
}

bool TextInputManager::canHandleInputEvent(const InputEventData &data) {
	if (_running && _isInputEnabled && data.key.compose != InputKeyComposeState::Disabled) {
		switch (data.event) {
		case InputEventName::KeyPressed:
		case InputEventName::KeyRepeated:
		case InputEventName::KeyReleased:
		case InputEventName::KeyCanceled:
			if (data.key.keychar || data.key.keycode == InputKeyCode::BACKSPACE || data.key.keycode == InputKeyCode::DELETE
					|| data.key.keycode == InputKeyCode::ESCAPE) {
				return true;
			}
			break;
		default:
			break;
		}
	}
	return false;
}

bool TextInputManager::handleInputEvent(const InputEventData &data) {
	switch (data.event) {
	case InputEventName::KeyPressed:
	case InputEventName::KeyRepeated:
		if (data.key.keycode == InputKeyCode::BACKSPACE || data.key.keychar == char32_t(0x0008)) {
			deleteBackward();
			return true;
		} else if (data.key.keycode == InputKeyCode::DELETE || data.key.keychar == char32_t(0x007f)) {
			deleteForward();
			return true;
		} else if (data.key.keycode == InputKeyCode::ESCAPE) {
			cancel();
		} else if (data.key.keychar) {
			auto c = data.key.keychar;
			// replace \r with \n for formatter
			if (c == '\r') {
				c = '\n';
			}
			switch (data.key.compose) {
			case InputKeyComposeState::Nothing:
				if (_compose == InputKeyComposeState::Composing) {
					_cursor.start += _cursor.length;
					_cursor.length = 0;
				}
				insertText(string::toUtf16<Interface>(c));
				break;
			case InputKeyComposeState::Composed:
				insertText(string::toUtf16<Interface>(c), false);
				break;
			case InputKeyComposeState::Composing:
				insertText(string::toUtf16<Interface>(c), true);
				break;
			case InputKeyComposeState::Disabled:
				break;
			}
			_compose = data.key.compose;
			return true;
		}
		break;
	case InputEventName::KeyReleased:
	case InputEventName::KeyCanceled:
		break;
	default:
		break;
	}
	return false;
}

bool TextInputManager::doInsertText(WideStringView sInsert, bool compose) {
	if (sInsert.size() > 0) {
		if (_cursor.length > 0 && (!compose || _compose != InputKeyComposeState::Composing)) {
			_string.erase(_string.begin() + _cursor.start, _string.begin() + _cursor.start + _cursor.length);
			_cursor.length = 0;
		}

		WideString sText(_string.substr(0, _cursor.start).append(sInsert.data(), sInsert.size()));

		if (_cursor.start < _string.length()) {
			sText.append(_string.substr(_cursor.start));
		}

		_string = std::move(sText);

		if (compose) {
			_cursor.length += sInsert.size();
		} else {
			_cursor.start += sInsert.size();
		}

		return true;
	}
	return false;
}

}
