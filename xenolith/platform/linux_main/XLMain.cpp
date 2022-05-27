
#include "XLDefine.h"
#include "XLPlatform.h"
#include "XLApplication.h"

#if MODULE_COMMON_BACKTRACE
#include "backtrace.h"
#include "signal.h"
#include "cxxabi.h"
#endif

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

int parseOptionSwitch(Value &ret, char c, const char *str) {
	return 1;
}

void sp_android_terminate () {
	stappler::log::text("Application", "Crash on exceprion");
  __gnu_cxx::__verbose_terminate_handler();
}

SP_EXTERN_C int _spMain(argc, argv) {
	memory::pool::initialize();
	std::set_terminate(sp_android_terminate);

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

	auto args = data::parseCommandLineOptions<Interface>(argc, argv, &parseOptionSwitch, &Application::parseOptionString);

    // create the application instance
    auto ret = Application::getInstance()->run(move(args));
	memory::pool::terminate();
	return ret;
}

}
