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

#ifndef COMPONENTS_XENOLITH_NODES_XLRESOURCECOMPONENT_H_
#define COMPONENTS_XENOLITH_NODES_XLRESOURCECOMPONENT_H_

#include "XLComponent.h"
#include "XLGlResource.h"

namespace stappler::xenolith {

class ResourceComponent : public Component {
public:
	virtual ~ResourceComponent() { }

	bool init(Rc<gl::Resource>);

	void setResource(Rc<gl::Resource>);
	Rc<gl::Resource> getResource() const;

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

protected:
	Rc<gl::Resource> _resource;
};

}

#endif /* COMPONENTS_XENOLITH_NODES_XLRESOURCECOMPONENT_H_ */
