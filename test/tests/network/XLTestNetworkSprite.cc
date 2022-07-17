/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLTestNetworkSprite.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLApplication.h"

namespace stappler::xenolith::test {

bool NetworkTestSprite::init() {
	if (!Sprite::init()) {
		return false;
	}

	setColor(Color::Orange_500);
	return true;
}

void NetworkTestSprite::onEnter(Scene *scene) {
	Sprite::onEnter(scene);

	auto app = scene->getDirector()->getApplication();

	/*auto h = Rc<network::DataHandle>::create("https://geobase.stappler.org/proxy/getHeaders");
	// h->addHeader("X-Test", "123");
	h->perform(app, [] (network::Handle &handle, data::Value &data) {
		auto url = handle.getUrl();
		auto code = handle.getErrorCode();
		auto err = handle.getError();
		auto resp = handle.getResponseCode();

		log::vtext("NetworkTest", "[", resp, "] ", url, ": ", code, " - ", err);
		std::cout << data::EncodeFormat::Pretty << data << "\n";
	});*/
}

}
