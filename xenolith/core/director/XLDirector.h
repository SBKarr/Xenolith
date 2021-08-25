/**
 Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_
#define COMPONENTS_XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_

#include "XLEventHeader.h"
#include "XLEventHandler.h"
#include "XLGlView.h"
#include "XLGlFrame.h"
#include "XLGlDrawScheme.h"

namespace stappler::xenolith {

class ResourceCache;
class Scene;

class Director : public Ref, EventHandler {
public:
	static EventHeader onProjectionChanged;
	static EventHeader onAfterUpdate;
	static EventHeader onAfterVisit;
	static EventHeader onAfterDraw;

	enum class Projection {
		_2D,
		_3D,
		Euclid,
		Custom,
		Default = Euclid,
	};

	static Rc<Director> getInstance();

	Director();

	virtual ~Director();
	virtual bool init();

	inline gl::View* getView() { return _view; }
	void setView(gl::View *view);

	void update(uint64_t);
	bool mainLoop(uint64_t);

	void render(const Rc<gl::FrameHandle> &);

	Rc<gl::DrawScheme> construct();

	void end();

	Size getScreenSize() const;
	Rc<ResourceCache> getResourceCache() const;

	void runScene(Rc<Scene>);

	virtual Rc<gl::RenderQueue> onDefaultRenderQueue(const gl::ImageInfo &);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	bool _running = false;
	Rc<gl::View> _view;

	Mutex _mutex;
	Rc<ResourceCache> _resourceCache;

	Rc<Scene> _scene;
	Rc<Scene> _nextScene;
	Mat4 _generalProjection;
	EventHandlerNode *_sizeChangedEvent = nullptr;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_ */
