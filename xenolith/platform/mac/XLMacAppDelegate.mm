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

#include "XLMacAppDelegate.h"
#include "XLMacViewController.h"
#include "XLApplication.h"
#include "XLPlatformMac.h"
#include "SPThread.h"

#import <QuartzCore/CAMetalLayer.h>

#if MACOS

static int parseOptionSwitch(stappler::xenolith::Value &ret, char c, const char *str) {
	return 1;
}

namespace stappler::xenolith {

class EngineMainThread : public thread::ThreadInterface<Interface> {
public:
	virtual ~EngineMainThread();
	
	bool init(Application *, Value &&);
	
	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

protected:
	Rc<Application> _application;
	Value _args;
	
	std::thread _thread;
	std::thread::id _threadId;
};

EngineMainThread::~EngineMainThread() {
	_application->end();
	_thread.join();
}

bool EngineMainThread::init(Application *app, Value &&args) {
	_application = app;
	_args = move(args);
	_thread = std::thread(EngineMainThread::workerThread, this, nullptr);
	return true;
}

void EngineMainThread::threadInit() {
	_threadId = std::this_thread::get_id();
}

void EngineMainThread::threadDispose() {
	
}

bool EngineMainThread::worker() {
	_application->run(move(_args));
	return false;
}

}

@implementation XLMacAppDelegate

- (instancetype _Nonnull) initWithArgc:(int)argc argv:(const char *_Nonnull [_Nonnull])argv {
	stappler::memory::pool::initialize();
	_arguments = stappler::data::parseCommandLineOptions<stappler::xenolith::Interface>(argc, (const char **)argv, &parseOptionSwitch, &stappler::xenolith::Application::parseOptionString);
	return self;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
	stappler::log::vtext("SPAppDelegate", "applicationDidFinishLaunching: ", stappler::data::EncodeFormat::Pretty, _arguments);

	_mainLoop = stappler::Rc<stappler::xenolith::EngineMainThread>::create(stappler::xenolith::Application::getInstance(), std::move(_arguments));
}

- (void)applicationWillTerminate:(NSNotification *)notification {
	_mainLoop = nullptr;
	stappler::memory::pool::terminate();
}

- (void)runEngineView: (const stappler::Rc<stappler::xenolith::platform::graphic::ViewImpl> &)view {
	auto rect = view->getFrame();
	_window = [[NSWindow alloc] initWithContentRect:NSRect{{static_cast<CGFloat>(rect.x), static_cast<CGFloat>(rect.y)}, {static_cast<CGFloat>(rect.width), static_cast<CGFloat>(rect.height)}}
								styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:YES];
	_window.title = [NSString stringWithUTF8String: view->getTitle().data()];
	_window.contentViewController = [[XLMacViewController alloc] initWithEngineView:view];
	_window.contentView = _window.contentViewController.view;
	[_window makeKeyAndOrderFront:nil];
}

- (void)appTest1Clicked {
	
}

- (void)appTest2Clicked {
	
}

@end

@implementation XLMacAppMenu

- (id) initWithTitle:(NSString *)title {
	self = [super initWithTitle:title];

	auto appMenuItem = [[NSMenuItem alloc] init];
	appMenuItem.title = @"MainMenu";
	appMenuItem.submenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
	auto item1 = [appMenuItem.submenu addItemWithTitle:@"AppTest1" action:@selector(appTest1Clicked) keyEquivalent:@""];
	auto item2 = [appMenuItem.submenu addItemWithTitle:@"AppTest2" action:@selector(appTest2Clicked) keyEquivalent:@""];
	
	item1.enabled = true;
	item2.enabled = true;
	
	auto fileMenuItem = [[NSMenuItem alloc] init];
	fileMenuItem.title = @"File";
	fileMenuItem.submenu = [[NSMenu alloc] initWithTitle:@"File"];
	[fileMenuItem.submenu addItemWithTitle:@"Test1" action:nil keyEquivalent:@""];
	[fileMenuItem.submenu addItemWithTitle:@"Test2" action:nil keyEquivalent:@""];
	
	self.itemArray = @[appMenuItem, fileMenuItem];

	return self;
}

- (id) initWithCoder:(NSCoder *)coder {
	return [super initWithCoder:coder];
}

@end

namespace stappler::xenolith::platform::graphic {

void ViewImpl_run(const Rc<ViewImpl> &view) {
	auto v = view.get();
	auto refId = v->retain();
	dispatch_async(dispatch_get_main_queue(), ^{
		[(XLMacAppDelegate *)NSApplication.sharedApplication.delegate runEngineView:v];
		v->release(refId);
	});
}

float ViewImpl_getScreenDensity() {
	return NSScreen.mainScreen.backingScaleFactor;
}

float ViewImpl_getSurfaceDensity(void *view) {
	return ((__bridge XLMacViewController *)view).view.layer.contentsScale;
}

void * ViewImpl_getLayer(void *view) {
	return (__bridge void *)((__bridge XLMacViewController *)view).view.layer;
}

void ViewImpl_wakeup(const Rc<ViewImpl> &view) {
	auto v = view.get();
	auto refId = v->retain();

	dispatch_async(dispatch_get_main_queue(), ^{
		v->update(false);
		v->release(refId);
	});
}

void ViewImpl_setVSyncEnabled(void *view, bool vsyncEnabled) {
	auto l = (CAMetalLayer *)((__bridge XLMacViewController *)view).view.layer;
	l.displaySyncEnabled = vsyncEnabled ? YES : NO;
}

void ViewImpl_updateTextCursor(void *view, uint32_t pos, uint32_t len) {
	auto v = (XLMacView *)((__bridge XLMacViewController *)view).view;
	[v updateTextCursorWithPosition: pos length:len];
}

void ViewImpl_updateTextInput(void *view, WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	auto v = (XLMacView *)((__bridge XLMacViewController *)view).view;
	[v updateTextInputWithText: str position:pos length:len type:type];
}

void ViewImpl_runTextInput(void *view, WideStringView str, uint32_t pos, uint32_t len, TextInputType type) {
	auto v = (XLMacView *)((__bridge XLMacViewController *)view).view;
	[v runTextInputWithText: str position:pos length:len type:type];
}

void ViewImpl_cancelTextInput(void *view) {
	auto v = (XLMacView *)((__bridge XLMacViewController *)view).view;
	[v cancelTextInput];
}

}

#endif
