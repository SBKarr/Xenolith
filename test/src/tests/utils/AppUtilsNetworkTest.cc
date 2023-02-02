/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "AppUtilsNetworkTest.h"
#include "XLNetworkRequest.h"

namespace stappler::xenolith::app {

bool UtilsNetworkTest::init() {
	if (!LayoutTest::init(LayoutName::UtilsNetworkTest, "")) {
		return false;
	}

	_background = addChild(Rc<MaterialBackground>::create(Color::BlueGrey_500), 1);
	_background->setAnchorPoint(Anchor::Middle);

	_runButton = _background->addChild(Rc<material::Button>::create(material::NodeStyle::Filled));
	_runButton->setText("Run");
	_runButton->setAnchorPoint(Anchor::MiddleTop);
	_runButton->setFollowContentSize(false);
	_runButton->setTapCallback([this] {
		performTest();
	});

	_result = _background->addChild(Rc<material::TypescaleLabel>::create(material::TypescaleRole::BodyLarge));
	_result->setFontFamily("default");
	_result->setString("null");
	_result->setAnchorPoint(Anchor::MiddleTop);

	return true;
}

void UtilsNetworkTest::onContentSizeDirty() {
	LayoutTest::onContentSizeDirty();

	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize);

	_runButton->setContentSize(Size2(120.0f, 32.0f));
	_runButton->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 72.0f));

	_result->setWidth(180.0f * 2.0f + 16.0f);
	_result->setPosition(Vec2(_contentSize.width / 2.0f, _contentSize.height - 120.0f));
}

void UtilsNetworkTest::performTest() {
	auto req = Rc<network::Request>::create([&] (network::Handle &handle) {
		handle.init(network::Method::Get, "https://geobase.stappler.org/proxy/getHeaders");
		handle.addHeader("X-Test", "123");
		handle.setVerifyTls(false);
		return true;
	}, this);

	req->perform(_director->getApplication(), [this] (const network::Request &req) {
		StringStream stream;
		auto data = data::read<Interface>(req.getData());

		stream << data::EncodeFormat::Pretty << data << "\n";
		_result->setString(stream.str());
	});
}

}
