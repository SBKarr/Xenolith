
#include "XLDefine.h"
#include "XLPlatform.h"
#include "XLApplication.h"

namespace stappler::xenolith {

int parseOptionSwitch(stappler::data::Value &ret, char c, const char *str) {
	return 1;
}

void sp_android_terminate () {
	stappler::log::text("Application", "Crash on exceprion");
  __gnu_cxx::__verbose_terminate_handler();
}

SP_EXTERN_C int _spMain(argc, argv) {
	std::set_terminate(sp_android_terminate);

	auto args = stappler::data::parseCommandLineOptions(argc, argv, &parseOptionSwitch, &Application::parseOptionString);

    // create the application instance
    return Application::getInstance()->run(move(args));
}

}
