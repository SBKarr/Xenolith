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
static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
									const CVTimeStamp* now,
									const CVTimeStamp* outputTime,
									CVOptionFlags flagsIn,
									CVOptionFlags* flagsOut,
									void* target) {
	((stappler::xenolith::platform::graphic::ViewImpl*)target)->handleDisplayLinkCallback();
	return kCVReturnSuccess;
}

@implementation XLMacViewController

- (instancetype _Nonnull) initWithEngineView: (const stappler::Rc<stappler::xenolith::platform::graphic::ViewImpl> &)view {
	self = [super init];
	_engineView = view;
	return self;
}

- (void)viewDidLoad {
	[super viewDidLoad];
}

- (void)viewDidAppear {
	[super viewDidAppear];

	stappler::log::text("XLMacViewController", "viewDidAppear");
}

- (void)viewWillDisappear {
	stappler::log::text("XLMacViewController", "viewWillDisappear");
	
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

	CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
	CVDisplayLinkSetOutputCallback(_displayLink, &DisplayLinkCallback, _engineView.get());
	CVDisplayLinkStart(_displayLink);
}

@end

@implementation XLMacView

- (BOOL)wantsUpdateLayer { return YES; }

+ (Class)layerClass { return [CAMetalLayer class]; }

- (CALayer *)makeBackingLayer {
	self.layer = [self.class.layerClass layer];

	CGSize viewScale = [self convertSizeToBacking:CGSizeMake(1.0, 1.0)];
	self.layer.contentsScale = MIN(viewScale.width, viewScale.height);
	return self.layer;
}

- (BOOL)acceptsFirstResponder { return YES; }

- (void) viewDidMoveToWindow {
	auto trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds] options:NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect owner:self userInfo:nil];
	[self addTrackingArea:trackingArea];
	
	self.window.delegate = (XLMacViewController *)self.window.contentViewController;
}

@end

#endif
