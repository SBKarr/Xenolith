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
#include "XLPlatformMac.h"
#include "XLMacViewController.h"

#import <QuartzCore/CAMetalLayer.h>

#if MACOS

static const NSRange kEmptyRange = { NSNotFound, 0 };

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
									const CVTimeStamp* now,
									const CVTimeStamp* outputTime,
									CVOptionFlags flagsIn,
									CVOptionFlags* flagsOut,
									void* target) {
	((platform::graphic::ViewImpl*)target)->handleDisplayLinkCallback();
	return kCVReturnSuccess;
}

static InputModifier GetInputModifiers(NSEventModifierFlags flags) {
	InputModifier mods = InputModifier::None;
	if ((flags & NSEventModifierFlagCapsLock) != 0) { mods |= InputModifier::CapsLock; }
	if ((flags & NSEventModifierFlagShift) != 0) { mods |= InputModifier::Shift; }
	if ((flags & NSEventModifierFlagControl) != 0) { mods |= InputModifier::Ctrl; }
	if ((flags & NSEventModifierFlagOption) != 0) { mods |= InputModifier::Alt; }
	if ((flags & NSEventModifierFlagNumericPad) != 0) { mods |= InputModifier::NumLock; }
	if ((flags & NSEventModifierFlagCommand) != 0) { mods |= InputModifier::Mod3; }
	if ((flags & NSEventModifierFlagHelp) != 0) { mods |= InputModifier::Mod4; }
	if ((flags & NSEventModifierFlagFunction) != 0) { mods |= InputModifier::Mod5; }
	return mods;
}

static InputMouseButton GetInputMouseButton(NSInteger buttonNumber) {
	switch (buttonNumber) {
		case 0: return InputMouseButton::MouseLeft;
		case 1: return InputMouseButton::MouseRight;
		case 2: return InputMouseButton::MouseMiddle;
		default:
			return InputMouseButton(stappler::toInt(InputMouseButton::Mouse8) + (buttonNumber - 3));
	}
}

static void createKeyTables(InputKeyCode keycodes[256], uint16_t scancodes[stappler::toInt(InputKeyCode::Max)]) {
	memset(keycodes, 0, sizeof(InputKeyCode) * 256);
	memset(scancodes, 0, sizeof(uint16_t) * stappler::toInt(InputKeyCode::Max));

	keycodes[0x1D] = InputKeyCode::_0;
	keycodes[0x12] = InputKeyCode::_1;
	keycodes[0x13] = InputKeyCode::_2;
	keycodes[0x14] = InputKeyCode::_3;
	keycodes[0x15] = InputKeyCode::_4;
	keycodes[0x17] = InputKeyCode::_5;
	keycodes[0x16] = InputKeyCode::_6;
	keycodes[0x1A] = InputKeyCode::_7;
	keycodes[0x1C] = InputKeyCode::_8;
	keycodes[0x19] = InputKeyCode::_9;
	keycodes[0x00] = InputKeyCode::A;
	keycodes[0x0B] = InputKeyCode::B;
	keycodes[0x08] = InputKeyCode::C;
	keycodes[0x02] = InputKeyCode::D;
	keycodes[0x0E] = InputKeyCode::E;
	keycodes[0x03] = InputKeyCode::F;
	keycodes[0x05] = InputKeyCode::G;
	keycodes[0x04] = InputKeyCode::H;
	keycodes[0x22] = InputKeyCode::I;
	keycodes[0x26] = InputKeyCode::J;
	keycodes[0x28] = InputKeyCode::K;
	keycodes[0x25] = InputKeyCode::L;
	keycodes[0x2E] = InputKeyCode::M;
	keycodes[0x2D] = InputKeyCode::N;
	keycodes[0x1F] = InputKeyCode::O;
	keycodes[0x23] = InputKeyCode::P;
	keycodes[0x0C] = InputKeyCode::Q;
	keycodes[0x0F] = InputKeyCode::R;
	keycodes[0x01] = InputKeyCode::S;
	keycodes[0x11] = InputKeyCode::T;
	keycodes[0x20] = InputKeyCode::U;
	keycodes[0x09] = InputKeyCode::V;
	keycodes[0x0D] = InputKeyCode::W;
	keycodes[0x07] = InputKeyCode::X;
	keycodes[0x10] = InputKeyCode::Y;
	keycodes[0x06] = InputKeyCode::Z;

	keycodes[0x27] = InputKeyCode::APOSTROPHE;
	keycodes[0x2A] = InputKeyCode::BACKSLASH;
	keycodes[0x2B] = InputKeyCode::COMMA;
	keycodes[0x18] = InputKeyCode::EQUAL;
	keycodes[0x32] = InputKeyCode::GRAVE_ACCENT;
	keycodes[0x21] = InputKeyCode::LEFT_BRACKET;
	keycodes[0x1B] = InputKeyCode::MINUS;
	keycodes[0x2F] = InputKeyCode::PERIOD;
	keycodes[0x1E] = InputKeyCode::RIGHT_BRACKET;
	keycodes[0x29] = InputKeyCode::SEMICOLON;
	keycodes[0x2C] = InputKeyCode::SLASH;
	keycodes[0x0A] = InputKeyCode::WORLD_1;

	keycodes[0x33] = InputKeyCode::BACKSPACE;
	keycodes[0x39] = InputKeyCode::CAPS_LOCK;
	keycodes[0x75] = InputKeyCode::DELETE;
	keycodes[0x7D] = InputKeyCode::DOWN;
	keycodes[0x77] = InputKeyCode::END;
	keycodes[0x24] = InputKeyCode::ENTER;
	keycodes[0x35] = InputKeyCode::ESCAPE;
	keycodes[0x7A] = InputKeyCode::F1;
	keycodes[0x78] = InputKeyCode::F2;
	keycodes[0x63] = InputKeyCode::F3;
	keycodes[0x76] = InputKeyCode::F4;
	keycodes[0x60] = InputKeyCode::F5;
	keycodes[0x61] = InputKeyCode::F6;
	keycodes[0x62] = InputKeyCode::F7;
	keycodes[0x64] = InputKeyCode::F8;
	keycodes[0x65] = InputKeyCode::F9;
	keycodes[0x6D] = InputKeyCode::F10;
	keycodes[0x67] = InputKeyCode::F11;
	keycodes[0x6F] = InputKeyCode::F12;
	keycodes[0x69] = InputKeyCode::PRINT_SCREEN;
	keycodes[0x6B] = InputKeyCode::F14;
	keycodes[0x71] = InputKeyCode::F15;
	keycodes[0x6A] = InputKeyCode::F16;
	keycodes[0x40] = InputKeyCode::F17;
	keycodes[0x4F] = InputKeyCode::F18;
	keycodes[0x50] = InputKeyCode::F19;
	keycodes[0x5A] = InputKeyCode::F20;
	keycodes[0x73] = InputKeyCode::HOME;
	keycodes[0x72] = InputKeyCode::INSERT;
	keycodes[0x7B] = InputKeyCode::LEFT;
	keycodes[0x3A] = InputKeyCode::LEFT_ALT;
	keycodes[0x3B] = InputKeyCode::LEFT_CONTROL;
	keycodes[0x38] = InputKeyCode::LEFT_SHIFT;
	keycodes[0x37] = InputKeyCode::LEFT_SUPER;
	keycodes[0x6E] = InputKeyCode::MENU;
	keycodes[0x47] = InputKeyCode::NUM_LOCK;
	keycodes[0x79] = InputKeyCode::PAGE_DOWN;
	keycodes[0x74] = InputKeyCode::PAGE_UP;
	keycodes[0x7C] = InputKeyCode::RIGHT;
	keycodes[0x3D] = InputKeyCode::RIGHT_ALT;
	keycodes[0x3E] = InputKeyCode::RIGHT_CONTROL;
	keycodes[0x3C] = InputKeyCode::RIGHT_SHIFT;
	keycodes[0x36] = InputKeyCode::RIGHT_SUPER;
	keycodes[0x31] = InputKeyCode::SPACE;
	keycodes[0x30] = InputKeyCode::TAB;
	keycodes[0x7E] = InputKeyCode::UP;

	keycodes[0x52] = InputKeyCode::KP_0;
	keycodes[0x53] = InputKeyCode::KP_1;
	keycodes[0x54] = InputKeyCode::KP_2;
	keycodes[0x55] = InputKeyCode::KP_3;
	keycodes[0x56] = InputKeyCode::KP_4;
	keycodes[0x57] = InputKeyCode::KP_5;
	keycodes[0x58] = InputKeyCode::KP_6;
	keycodes[0x59] = InputKeyCode::KP_7;
	keycodes[0x5B] = InputKeyCode::KP_8;
	keycodes[0x5C] = InputKeyCode::KP_9;
	keycodes[0x45] = InputKeyCode::KP_ADD;
	keycodes[0x41] = InputKeyCode::KP_DECIMAL;
	keycodes[0x4B] = InputKeyCode::KP_DIVIDE;
	keycodes[0x4C] = InputKeyCode::KP_ENTER;
	keycodes[0x51] = InputKeyCode::KP_EQUAL;
	keycodes[0x43] = InputKeyCode::KP_MULTIPLY;
	keycodes[0x4E] = InputKeyCode::KP_SUBTRACT;

	for (int scancode = 0;  scancode < 256;  scancode++) {
		// Store the reverse translation for faster key name lookup
		if (stappler::toInt(keycodes[scancode]) >= 0) {
			scancodes[stappler::toInt(keycodes[scancode])] = scancode;
		}
	}
}

@implementation XLMacViewController

- (instancetype _Nonnull) initWithEngineView: (const stappler::Rc<platform::graphic::ViewImpl> &)view {
	self = [super init];
	_engineView = view;
	createKeyTables(keycodes, scancodes);
	return self;
}

- (void)viewDidLoad {
	[super viewDidLoad];
}

- (void)updateEngineView {
	_engineView->update(false);
}

- (void)viewDidAppear {
	[super viewDidAppear];

	_updateTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 1000000.0) * 500.0
		target:self
		selector:@selector(updateEngineView)
		userInfo:nil
		repeats:NO];
}

- (void)viewWillDisappear {
	[_updateTimer invalidate];
	_updateTimer = nil;

	[super viewWillDisappear];
}

- (void)viewDidDisappear {
	[super viewDidDisappear];

	CVDisplayLinkStop(_displayLink);
	CVDisplayLinkRelease(_displayLink);

	_engineView->threadDispose();
	_engineView->setOsView(nullptr);
	_engineView = nullptr;
}

- (void)loadView {
	auto rect = _engineView->getFrame();
	self.view = [[XLMacView alloc] initWithFrame:NSRect{{static_cast<CGFloat>(rect.x), static_cast<CGFloat>(rect.y)}, {static_cast<CGFloat>(rect.width), static_cast<CGFloat>(rect.height)}}];

	self.view.wantsLayer = YES;

	_engineView->setOsView((__bridge void *)self);
	_engineView->threadInit();

	_currentSize = NSSizeFromCGSize(CGSizeMake(rect.width, rect.height));

	CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
	CVDisplayLinkSetOutputCallback(_displayLink, &DisplayLinkCallback, _engineView.get());
	CVDisplayLinkStart(_displayLink);
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
	return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
	auto size = self.view.window.contentLayoutRect.size;
	if (_currentSize.width != size.width || _currentSize.height != size.height) {
		_engineView->update(false);
		_engineView->deprecateSwapchain(false);
		_currentSize = size;
	}
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
	_engineView->startLiveResize();
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
	_engineView->stopLiveResize();
}

- (void)mouseDown:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Begin,
		stappler::xenolith::InputMouseButton::MouseLeft,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)rightMouseDown:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Begin,
		stappler::xenolith::InputMouseButton::MouseRight,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)otherMouseDown:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Begin,
		GetInputMouseButton(theEvent.buttonNumber),
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)mouseUp:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::End,
		stappler::xenolith::InputMouseButton::MouseLeft,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)rightMouseUp:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::End,
		stappler::xenolith::InputMouseButton::MouseRight,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)otherMouseUp:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::End,
		GetInputMouseButton(theEvent.buttonNumber),
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)mouseMoved:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		std::numeric_limits<uint32_t>::max(),
		stappler::xenolith::InputEventName::MouseMove,
		stappler::xenolith::InputMouseButton::None,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)mouseDragged:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	
	stappler::xenolith::Vector<stappler::xenolith::InputEventData> events;
	
	events.emplace_back(stappler::xenolith::InputEventData({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Move,
		stappler::xenolith::InputMouseButton::MouseLeft,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	}));
	events.emplace_back(stappler::xenolith::InputEventData({
		std::numeric_limits<uint32_t>::max(),
		stappler::xenolith::InputEventName::MouseMove,
		stappler::xenolith::InputMouseButton::None,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	}));

	_engineView->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)scrollWheel:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];

	stappler::xenolith::Vector<stappler::xenolith::InputEventData> events;

	uint32_t buttonId = 0;
	stappler::xenolith::InputMouseButton buttonName;
	auto mods = GetInputModifiers(theEvent.modifierFlags);

	if (theEvent.scrollingDeltaY != 0) {
		if (theEvent.scrollingDeltaY > 0) {
			buttonName = InputMouseButton::MouseScrollUp;
		} else {
			buttonName = InputMouseButton::MouseScrollDown;
		}
	} else {
		if (theEvent.scrollingDeltaX > 0) {
			buttonName = InputMouseButton::MouseScrollRight;
		} else {
			buttonName = InputMouseButton::MouseScrollLeft;
		}
	}

	buttonId = stappler::maxOf<uint32_t>() - stappler::toInt(buttonName);

	events.emplace_back(stappler::xenolith::InputEventData({
		buttonId,
		stappler::xenolith::InputEventName::Begin,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y)
	}));
	events.emplace_back(stappler::xenolith::InputEventData({
		buttonId,
		stappler::xenolith::InputEventName::Scroll,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y)
	}));
	events.emplace_back(stappler::xenolith::InputEventData({
		buttonId,
		stappler::xenolith::InputEventName::End,
		buttonName,
		mods,
		float(loc.x),
		float(loc.y)
	}));

	events.at(1).point.valueX = theEvent.scrollingDeltaX;
	events.at(1).point.valueY = theEvent.scrollingDeltaY;
	events.at(1).point.density = 1.0f;
	
	_engineView->handleInputEvents(move(events));
	_currentPointerLocation = loc;
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Move,
		stappler::xenolith::InputMouseButton::MouseRight,
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(theEvent.buttonNumber),
		stappler::xenolith::InputEventName::Move,
		GetInputMouseButton(theEvent.buttonNumber), 
		GetInputModifiers(theEvent.modifierFlags),
		float(loc.x),
		float(loc.y)
	});

	_engineView->handleInputEvent(event);
	_currentPointerLocation = loc;
}

- (void)mouseEntered:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];

	_engineView->handleInputEvent(stappler::xenolith::InputEventData::BoolEvent(stappler::xenolith::InputEventName::PointerEnter, true, Vec2(loc.x, loc.y)));
	_currentPointerLocation = loc;
}

- (void)mouseExited:(NSEvent *)theEvent {
	auto loc = [self.view convertPointToBacking: [self.view convertPoint:theEvent.locationInWindow fromView:nil]];

	_engineView->handleInputEvent(stappler::xenolith::InputEventData::BoolEvent(stappler::xenolith::InputEventName::PointerEnter, false, Vec2(loc.x, loc.y)));
	_currentPointerLocation = loc;
}

- (BOOL)becomeFirstResponder {
	return [self becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
	return [self resignFirstResponder];
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
	_engineView->handleInputEvent(stappler::xenolith::InputEventData::BoolEvent(stappler::xenolith::InputEventName::FocusGain, true, Vec2(_currentPointerLocation.x, _currentPointerLocation.y)));
}

- (void)windowDidResignKey:(NSNotification *)notification {
	_engineView->handleInputEvent(stappler::xenolith::InputEventData::BoolEvent(stappler::xenolith::InputEventName::FocusGain, false, Vec2(_currentPointerLocation.x, _currentPointerLocation.y)));
}

- (void)viewDidChangeBackingProperties {
	CGSize viewScale = [self.view convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	//self.layer.contentsScale = MIN(viewScale.width, viewScale.height);
	std::cout << viewScale.width << " " << viewScale.height << "\n";
}

- (void)keyDown:(NSEvent *)theEvent {
	auto code = [theEvent keyCode];
	
	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(code),
		theEvent.isARepeat ? InputEventName::KeyRepeated : InputEventName::KeyPressed,
		GetInputMouseButton(theEvent.buttonNumber),
		GetInputModifiers(theEvent.modifierFlags),
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)
	});

	event.key.keycode = keycodes[static_cast<uint8_t>(code)];
	event.key.compose = InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;
	_engineView->handleInputEvent(event);

	std::cout << (theEvent.isARepeat ? "keyRepeat: " : "keyDown: ") << code << " " << getInputKeyCodeName(event.key.keycode) << "\n";
}

- (void)keyUp:(NSEvent *)theEvent {
	auto code = [theEvent keyCode];

	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(code),
		stappler::xenolith::InputEventName::KeyReleased,
		stappler::xenolith::InputMouseButton::None,
		GetInputModifiers(theEvent.modifierFlags),
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)
	});

	event.key.keycode = keycodes[static_cast<uint8_t>(code)];
	event.key.compose = InputKeyComposeState::Disabled;
	event.key.keysym = 0;
	event.key.keychar = 0;
	_engineView->handleInputEvent(event);

	std::cout << "keyUp: " << code << " " << getInputKeyCodeName(event.key.keycode) << "\n";
}

- (void)flagsChanged:(NSEvent *)theEvent {
	static std::pair<uint32_t, InputKeyCode> testmask [] = {
		std::make_pair(NX_DEVICELSHIFTKEYMASK, InputKeyCode::LEFT_SHIFT),
		std::make_pair(NX_DEVICERSHIFTKEYMASK, InputKeyCode::RIGHT_SHIFT),
		std::make_pair(NX_DEVICELCTLKEYMASK, InputKeyCode::LEFT_CONTROL),
		std::make_pair(NX_DEVICERCTLKEYMASK, InputKeyCode::RIGHT_CONTROL),
		std::make_pair(NX_DEVICELALTKEYMASK, InputKeyCode::LEFT_ALT),
		std::make_pair(NX_DEVICERALTKEYMASK, InputKeyCode::RIGHT_ALT),
		std::make_pair(NX_DEVICELCMDKEYMASK, InputKeyCode::LEFT_SUPER),
		std::make_pair(NX_DEVICERCMDKEYMASK, InputKeyCode::RIGHT_SUPER),
		std::make_pair(NSEventModifierFlagCapsLock, InputKeyCode::CAPS_LOCK),
		std::make_pair(NSEventModifierFlagNumericPad, InputKeyCode::NUM_LOCK),
		std::make_pair(NSEventModifierFlagFunction, InputKeyCode::WORLD_1),
		std::make_pair(NSEventModifierFlagHelp, InputKeyCode::WORLD_2),
	};

	auto mods = theEvent.modifierFlags;
	if (mods == _currentModifiers) {
		return;
	}
	
	auto diff = mods ^ _currentModifiers;

	stappler::xenolith::InputEventData event({
		static_cast<uint32_t>(0),
		stappler::xenolith::InputEventName::KeyReleased,
		stappler::xenolith::InputMouseButton::None,
		GetInputModifiers(theEvent.modifierFlags),
		float(_currentPointerLocation.x),
		float(_currentPointerLocation.y)
	});

	for (auto &it : testmask) {
		if ((diff & it.first) != 0) {
			event.id = it.first;
			if ((mods & it.first) != 0) {
				event.event = InputEventName::KeyPressed;
			}
			event.key.keycode = it.second;
			event.key.compose = InputKeyComposeState::Disabled;
			event.key.keysym = 0;
			event.key.keychar = 0;
			break;
		}
	}
	
	if (event.id) {
		_engineView->handleInputEvent(event);
	}
	
	_currentModifiers = mods;
}

- (void) submitTextData:(WideStringView)str cursor:(TextCursor)cursor marked:(TextCursor)marked {
	_engineView->submitTextData(str, cursor, marked);
}

@end

@implementation XLMacView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (instancetype)initWithFrame:(NSRect)frameRect {
	self = [super initWithFrame:frameRect];
	_validAttributesForMarkedText = [NSArray array];
	_textInput = stappler::Rc<TextInputManager>::create(&_textInterface);
	_textDirty = false;
	_textHandler.onText = [self] (WideStringView str, TextCursor cursor, TextCursor marked) {
		_textDirty = true;
	};
	_textHandler.onInput = [] (bool) {
		
	};
	_textHandler.onEnded = [] () {
		
	};
	return self;
}

- (BOOL)wantsUpdateLayer { return YES; }

- (CALayer *)makeBackingLayer {
	self.layer = [self.class.layerClass layer];

	CGSize viewScale = [self convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	self.layer.contentsScale = MIN(viewScale.width, viewScale.height);
	return self.layer;
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void) viewDidMoveToWindow {
	auto trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited owner:self userInfo:nil];
	[self addTrackingArea:trackingArea];

	self.window.delegate = (XLMacViewController *)self.window.contentViewController;
}

- (void)keyDown:(NSEvent *)theEvent {
	TextInputManager *m = _textInput.get();
	if (m->isInputEnabled()) {
		[self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
		if (_textDirty) {
			[(XLMacViewController *)self.window.contentViewController submitTextData:m->getString() cursor:m->getCursor() marked:m->getMarked()];
			_textDirty = false;
		}
	}
	[super keyDown:theEvent];
}

- (void)deleteForward:(nullable id)sender {
	std::cout << "deleteForward\n";
	TextInputManager *m = _textInput.get();
	m->deleteForward();
}

- (void)deleteBackward:(nullable id)sender {
	std::cout << "deleteBackward\n";
	TextInputManager *m = _textInput.get();
	m->deleteBackward();
}

- (void)insertTab:(nullable id)sender {
	TextInputManager *m = _textInput.get();
	m->insertText(u"\t");
}

- (void)insertBacktab:(nullable id)sender {
	
}

- (void)insertNewline:(nullable id)sender {
	TextInputManager *m = _textInput.get();
	m->insertText(u"\n");
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. */
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	WideString str; str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++ i) {
		str.push_back([characters characterAtIndex:i]);
	}

	TextInputManager *m = _textInput.get();
	m->insertText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)));
}

/* The receiver inserts string replacing the content specified by replacementRange. string can be either an NSString or NSAttributedString instance. selectedRange specifies the selection inside the string being inserted; hence, the location is relative to the beginning of string. When string is an NSString, the receiver is expected to render the marked text with distinguishing appearance (i.e. NSTextView renders with -markedTextAttributes). */
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
	std::cout << "setMarkedText\n";
	NSString *characters;
	if ([string isKindOfClass:[NSAttributedString class]]) {
		characters = [(NSAttributedString *)string string];
	} else {
		characters = string;
	}

	WideString str; str.reserve(characters.length);
	for (size_t i = 0; i < characters.length; ++ i) {
		str.push_back([characters characterAtIndex:i]);
	}

	TextInputManager *m = _textInput.get();
	m->setMarkedText(str, TextCursor(uint32_t(replacementRange.location), uint32_t(replacementRange.length)), TextCursor(uint32_t(selectedRange.location), uint32_t(selectedRange.length)));
}

/* The receiver unmarks the marked text. If no marked text, the invocation of this method has no effect. */
- (void)unmarkText {
	TextInputManager *m = _textInput.get();
	m->unmarkText();
}

/* Returns the selection range. The valid location is from 0 to the document length. */
- (NSRange)selectedRange {
	TextInputManager *m = _textInput.get();
	auto cursor = m->getCursor();
	if (cursor.length > 0) {
		return NSRange{cursor.start, cursor.length};
	}
	return kEmptyRange;
}

/* Returns the marked range. Returns {NSNotFound, 0} if no marked range. */
- (NSRange)markedRange {
	TextInputManager *m = _textInput.get();
	auto cursor = m->getMarked();
	if (cursor.length > 0) {
		return NSRange{cursor.start, cursor.length};
	}
	return kEmptyRange;
}

/* Returns whether or not the receiver has marked text. */
- (BOOL)hasMarkedText {
	TextInputManager *m = _textInput.get();
	auto cursor = m->getMarked();
	if (cursor.length > 0) {
		return YES;
	}
	return NO;
}

/* Returns attributed string specified by range. It may return nil. If non-nil return value and actualRange is non-NULL, it contains the actual range for the return value. The range can be adjusted from various reasons (i.e. adjust to grapheme cluster boundary, performance optimization, etc). */
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
	std::cout << "attributedSubstringForProposedRange\n";
	TextInputManager *m = _textInput.get();
	WideStringView str = m->getStringByRange(TextCursor{uint32_t(range.location), uint32_t(range.length)});
	if (actualRange != nil) {
		auto fullString = m->getString();

		actualRange->location = (str.data() - fullString.data());
		actualRange->length = str.size();
	}

	return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
}

/* Returns an array of attribute names recognized by the receiver.
*/
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
	return _validAttributesForMarkedText;
}

/* Returns the first logical rectangular area for range. The return value is in the screen coordinate. The size value can be negative if the text flows to the left. If non-NULL, actuallRange contains the character range corresponding to the returned area.
*/
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
	std::cout << "firstRectForCharacterRange\n";
	return self.frame;
}

/* Returns the index for character that is nearest to point. point is in the screen coordinate system.
*/
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
	std::cout << "characterIndexForPoint\n";
	return 0;
}

- (NSAttributedString *)attributedString {
	std::cout << "attributedString\n";
	TextInputManager *m = _textInput.get();
	auto str = m->getString();

	return [[NSAttributedString alloc] initWithString:[NSString stringWithCharacters:(const unichar *)str.data() length:str.size()]];
}

/* Returns the fraction of distance for point from the left side of the character. This allows caller to perform precise selection handling.
*/
- (CGFloat)fractionOfDistanceThroughGlyphForPoint:(NSPoint)point {
	std::cout << "fractionOfDistanceThroughGlyphForPoint\n";
	return 0.0f;
}

/* Returns the baseline position relative to the origin of rectangle returned by -firstRectForCharacterRange:actualRange:. This information allows the caller to access finer-grained character position inside the NSTextInputClient document.
*/
- (CGFloat)baselineDeltaForCharacterAtIndex:(NSUInteger)anIndex {
	std::cout << "baselineDeltaForCharacterAtIndex\n";
	return 0.0f;
}

/* Returns if the marked text is in vertical layout.
 */
- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
	std::cout << "drawsVerticallyForCharacterAtIndex\n";
	return NO;
}

- (void)updateTextCursorWithPosition:(uint32_t)pos length:(uint32_t)len {
	TextInputManager *m = _textInput.get();
	m->cursorChanged(TextCursor(pos, len));
}

- (void)updateTextInputWithText:(WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(TextInputType)type {
	_textDirty = false;
	TextInputManager *m = _textInput.get();
	m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
}

- (void)runTextInputWithText:(WideStringView)str position:(uint32_t)pos length:(uint32_t)len type:(TextInputType)type {
	_textDirty = false;
	TextInputManager *m = _textInput.get();
	m->run(&_textHandler, str, TextCursor(pos, len), TextCursor::InvalidCursor, type);
	m->setInputEnabled(true);
}

- (void)cancelTextInput {
	TextInputManager *m = _textInput.get();
	m->cancel();
	_textDirty = false;
}

@end

#endif
