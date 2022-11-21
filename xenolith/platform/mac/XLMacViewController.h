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

#include "XLDefine.h"

#if MACOS

#include "XLTextInputManager.h"
#include "XLPlatformMac.h"

#import <Cocoa/Cocoa.h>
#import <IOKit/hid/IOHIDLib.h>

using namespace stappler::xenolith;

@interface XLMacViewController : NSViewController <NSWindowDelegate> {
	CVDisplayLinkRef _displayLink;
	stappler::Rc<platform::graphic::ViewImpl> _engineView;
	NSSize _currentSize;
	NSPoint _currentPointerLocation;
	NSTimer *_updateTimer;

	NSEventModifierFlags _currentModifiers;
	InputKeyCode keycodes[256];
	uint16_t scancodes[128];
}

- (instancetype _Nonnull) initWithEngineView: (const stappler::Rc<stappler::xenolith::platform::graphic::ViewImpl> &)view;

- (void) submitTextData:(WideStringView)str cursor:(TextCursor)cursor marked:(TextCursor)marked;

@end

class XLMacTextInputInterface : public TextInputViewInterface {
public:
	virtual ~XLMacTextInputInterface() { }

	virtual void updateTextCursor(uint32_t pos, uint32_t len) { }
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) { }
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) { }
	virtual void cancelTextInput() { }
};

@interface XLMacView : NSView <NSTextInputClient> {
	NSArray<NSAttributedStringKey> *_validAttributesForMarkedText;
	XLMacTextInputInterface _textInterface;
	stappler::Rc<TextInputManager> _textInput;
	TextInputHandler _textHandler;
	bool _textDirty;
}

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len;

- (void)updateTextInputWithText:(WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(TextInputType)type;

- (void)runTextInputWithText:(WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(TextInputType)type;

- (void)cancelTextInput;

@end

#endif
