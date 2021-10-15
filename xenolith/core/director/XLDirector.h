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

	Director();

	virtual ~Director();
	virtual bool init(Application *, Rc<Scene> &&);

	gl::View *getView() const { return _view; }
	Application *getApplication() const { return _application; }
	const Rc<Scene> &getScene() const { return _scene; }

	void update();

	void acquireInput(gl::FrameHandle &, const Rc<gl::AttachmentHandle> &);

	Rc<gl::DrawScheme> construct();

	// should return valid RenderQueue from initial scene
	void begin(gl::View *view);
	void end();

	Size getScreenSize() const;

	void runScene(Rc<Scene> &&);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	uint64_t _lastTime = 0;
	bool _running = false;
	Rc<gl::View> _view;

	Mutex _mutex;

	Application *_application = nullptr;
	Rc<Scene> _scene;
	Rc<Scene> _nextScene;

	Mat4 _generalProjection;
	EventHandlerNode *_sizeChangedEvent = nullptr;
};

}

#endif /* COMPONENTS_XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_ */
