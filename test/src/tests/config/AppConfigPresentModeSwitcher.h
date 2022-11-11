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

#ifndef TEST_SRC_TESTS_CONFIG_APPCONFIGPRESENTMODESWITCHER_H_
#define TEST_SRC_TESTS_CONFIG_APPCONFIGPRESENTMODESWITCHER_H_

#include "XLLayer.h"
#include "XLVectorSprite.h"

namespace stappler::xenolith::app {

class AppDelegate;

class ConfigSwitcher : public Node {
public:
	virtual ~ConfigSwitcher() { }

	virtual bool init(AppDelegate *, uint32_t, Function<void(uint32_t)> &&);

	virtual void onContentSizeDirty() override;

protected:
	using Node::init;

	virtual uint32_t getCurrentValue(AppDelegate *) const = 0;
	virtual Vector<uint32_t> getValueList(AppDelegate *) const = 0;
	virtual String getValueLabel(uint32_t) const = 0;

	void updateState();
	void applyGradient(Layer *, const SimpleGradient &);

	void handlePrevMode();
	void handleNextMode();

	Label *_label = nullptr;
	Vector<Layer *> _layers;
	uint32_t _presentIndex = 0;
	Vector<uint32_t> _values;

	uint32_t _currentMode = 0;
	uint32_t _selectedMode = 0;

	VectorSprite *_left = nullptr;
	VectorSprite *_right = nullptr;

	Layer *_layerLeft = nullptr;
	Layer *_layerRight = nullptr;

	Function<void(uint32_t)> _callback;

	bool _selectedLeft = false;
	bool _selectedRight = false;
};

class ConfigPresentModeSwitcher : public ConfigSwitcher {
public:
	virtual ~ConfigPresentModeSwitcher() { }

	virtual bool init(AppDelegate *, uint32_t, Function<void(uint32_t)> &&) override;

protected:
	virtual uint32_t getCurrentValue(AppDelegate *) const override;
	virtual Vector<uint32_t> getValueList(AppDelegate *) const override;
	virtual String getValueLabel(uint32_t) const override;

	void updateAppData(AppDelegate *);
};

}

#endif /* TEST_SRC_TESTS_CONFIG_APPCONFIGPRESENTMODESWITCHER_H_ */
