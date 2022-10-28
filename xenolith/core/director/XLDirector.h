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

#ifndef XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_
#define XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_

#include "XLEventHeader.h"
#include "XLEventHandler.h"
#include "XLResourceCache.h"
#include "XLGlView.h"

namespace stappler::xenolith {

class Scene;
class Scheduler;
class InputDispatcher;
class TextInputManager;
class ActionManager;

class Director : public Ref, EventHandler {
public:
	using FrameRequest = renderqueue::FrameRequest;

	virtual ~Director();

	Director();

	bool init(Application *, gl::View *);

	void runScene(Rc<Scene> &&);

	bool acquireFrame(const Rc<FrameRequest> &);

	void update();

	void end();

	gl::View *getView() const { return _view; }
	Application *getApplication() const { return _application; }
	Scheduler *getScheduler() const { return _scheduler; }
	ActionManager *getActionManager() const { return _actionManager; }
	InputDispatcher *getInputDispatcher() const { return _inputDispatcher; }
	TextInputManager *getTextInputManager() const;

	const Rc<Scene> &getScene() const { return _scene; }
	const Rc<ResourceCache> &getResourceCache() const;
	const Mat4 &getGeneralProjection() const { return _generalProjection; }

	Extent2 getScreenExtent() const { return _screenExtent; }
	Size2 getScreenSize() const { return _screenSize; }

	void pushDrawStat(const gl::DrawStat &);

	const gl::DrawStat &getDrawStat() const { return _drawStat; }

	float getFps() const;
	float getAvgFps() const;
	float getSpf() const; // in milliseconds
	float getLocalFrameTime() const; // in milliseconds

	void autorelease(Ref *);

protected:
	// Vk Swaphain was invalidated, drop all dependent resources;
	void invalidate();

	void updateGeneralTransform();

	Extent2 _screenExtent;
	Size2 _screenSize;

	uint64_t _startTime = 0;
	UpdateTime _time;
	gl::DrawStat _drawStat;

	Mutex _mutex;

	Application *_application = nullptr;
	gl::View *_view = nullptr;

	Rc<Scene> _scene;
	Rc<Scene> _nextScene;

	Mat4 _generalProjection;
	EventHandlerNode *_sizeChangedEvent = nullptr;

	Rc<PoolRef> _pool;
	Rc<Scheduler> _scheduler;
	Rc<ActionManager> _actionManager;
	Rc<InputDispatcher> _inputDispatcher;

	Vector<Rc<Ref>> _autorelease;
};

}

#endif /* XENOLITH_CORE_DIRECTOR_XLDIRECTOR_H_ */
