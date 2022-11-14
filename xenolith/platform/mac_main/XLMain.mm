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
#include "XLPlatform.h"
#include "XLApplication.h"

#if MODULE_COMMON_BACKTRACE
#include "backtrace.h"
#include "signal.h"
#include "cxxabi.h"
#endif

#include "XLMacAppDelegate.h"

namespace stappler::xenolith {

#if MODULE_COMMON_BACKTRACE
static ::backtrace_state *s_backtraceState;
static struct sigaction s_sharedSigAction;
static struct sigaction s_sharedSigOldAction;

static void debug_backtrace_error(void *data, const char *msg, int errnum) {
	std::cout << "Backtrace error: " << msg << "\n";
}

static int debug_backtrace_full_callback(void *data, uintptr_t pc, const char *filename, int lineno, const char *function) {
	FILE *f = (FILE *)data;

	fprintf(f, "\t[%p]", (void *)pc);

	if (filename) {
		auto name = filepath::name(filename);
		fprintf(f, " %s:%d", name.data(), lineno);
	}
	if (function) {
		int status = 0;
		auto ptr = abi::__cxa_demangle (function, nullptr, nullptr, &status);
		if (ptr) {
			fprintf(f, " - %s", ptr);
			::free(ptr);
		} else {
			fprintf(f, " - %s", function);
		}
	}
	fprintf(f, "\n");
	return 0;
}

static void PrintBacktrace(FILE *f, int len) {
	backtrace_full(s_backtraceState, 2, debug_backtrace_full_callback, debug_backtrace_error, f);
}

static void s_sigAction(int sig, siginfo_t *info, void *ucontext) {
	PrintBacktrace(stdout, 100);
	abort();
}

#endif

static XLMacAppDelegate *delegate = nil;

SP_EXTERN_C int _spMain(argc, argv) {
#if MODULE_COMMON_BACKTRACE
	if (!s_backtraceState) {
		s_backtraceState = backtrace_create_state(nullptr, 1, debug_backtrace_error, nullptr);
	}

	memset(&s_sharedSigAction, 0, sizeof(s_sharedSigAction));
	s_sharedSigAction.sa_sigaction = &s_sigAction;
	s_sharedSigAction.sa_flags = SA_SIGINFO;
	sigemptyset(&s_sharedSigAction.sa_mask);
	//sigaddset(&s_sharedSigAction.sa_mask, SIGSEGV);

    ::sigaction(SIGSEGV, &s_sharedSigAction, &s_sharedSigOldAction);
#endif

	delegate = [[XLMacAppDelegate alloc] initWithArgc: argc argv:argv];

	NSApplication.sharedApplication.activationPolicy = NSApplicationActivationPolicyRegular;
	NSApplication.sharedApplication.mainMenu = [[XLMacAppMenu alloc] init];
	NSApplication.sharedApplication.presentationOptions = NSApplicationPresentationDefault;
	NSApplication.sharedApplication.delegate = delegate;
	
	NSApplicationMain(argc, argv);

	delegate = nil;
	return 0;
}

}
