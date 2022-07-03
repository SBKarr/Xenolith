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

#ifndef XENOLITH_NODES_XLSTRICTNODE_H_
#define XENOLITH_NODES_XLSTRICTNODE_H_

#include "XLNode.h"

namespace stappler::xenolith {

class StrictNode : public Node {
public:
	virtual bool init() override;

	virtual bool isClippingEnabled() const { return _isClippingEnabled; }
	virtual void setClippingEnabled(bool value);

	virtual bool isClipOnlyNodesBelow() const { return _clipOnlyNodesBelow; }
	virtual void setClipOnlyNodesBelow(bool value);

	virtual void setOutline(const Padding &);
	virtual Padding getOutline() const { return _outline; }

protected:
	bool _isClippingEnabled = true;
	bool _clipOnlyNodesBelow = false;
	Padding _outline;
};

}

#endif /* XENOLITH_NODES_XLSTRICTNODE_H_ */
