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

#ifndef XENOLITH_FEATURES_ACTIONS_XLACTIONEASE_H_
#define XENOLITH_FEATURES_ACTIONS_XLACTIONEASE_H_

#include "XLAction.h"

namespace stappler::xenolith::interpolation {

enum Type {
	Linear,

	Sine_EaseIn,
	Sine_EaseOut,
	Sine_EaseInOut,

	Quad_EaseIn,
	Quad_EaseOut,
	Quad_EaseInOut,

	Cubic_EaseIn,
	Cubic_EaseOut,
	Cubic_EaseInOut,

	Quart_EaseIn,
	Quart_EaseOut,
	Quart_EaseInOut,

	Quint_EaseIn,
	Quint_EaseOut,
	Quint_EaseInOut,

	Expo_EaseIn,
	Expo_EaseOut,
	Expo_EaseInOut,

	Circ_EaseIn,
	Circ_EaseOut,
	Circ_EaseInOut,

	Elastic_EaseIn,
	Elastic_EaseOut,
	Elastic_EaseInOut,

	Back_EaseIn,
	Back_EaseOut,
	Back_EaseInOut,

	Bounce_EaseIn,
	Bounce_EaseOut,
	Bounce_EaseInOut,

	Custom,

	Max
};

float interpolateTo(float time, Type type, float *easingParam);

float linear(float time);

float easeIn(float time, float rate);
float easeOut(float time, float rate);
float easeInOut(float time, float rate);

float bezieratFunction(float t, float a, float b, float c, float d);

float quadraticIn(float time);
float quadraticOut(float time);
float quadraticInOut(float time);

float sineEaseIn(float time);
float sineEaseOut(float time);
float sineEaseInOut(float time);

float quadEaseIn(float time);
float quadEaseOut(float time);
float quadEaseInOut(float time);

float cubicEaseIn(float time);
float cubicEaseOut(float time);
float cubicEaseInOut(float time);

float quartEaseIn(float time);
float quartEaseOut(float time);
float quartEaseInOut(float time);

float quintEaseIn(float time);
float quintEaseOut(float time);
float quintEaseInOut(float time);

float expoEaseIn(float time);
float expoEaseOut(float time);
float expoEaseInOut(float time);

float circEaseIn(float time);
float circEaseOut(float time);
float circEaseInOut(float time);

float elasticEaseIn(float time, float period);
float elasticEaseOut(float time, float period);
float elasticEaseInOut(float time, float period);

float backEaseIn(float time);
float backEaseOut(float time);
float backEaseInOut(float time);

float bounceEaseIn(float time);
float bounceEaseOut(float time);
float bounceEaseInOut(float time);

float customEase(float time, float *easingParam);

}

namespace stappler::xenolith {

class ActionEase : public ActionInterval {
public:
	virtual ~ActionEase();

	bool init(ActionInterval *action);

	virtual void startWithTarget(Node *target) override;
	virtual void stop() override;
	virtual void update(float time) override;

protected:
	Rc<ActionInterval> _inner;
};

class EaseRateAction : public ActionEase {
public:
	virtual ~EaseRateAction();

	bool init(ActionInterval *pAction, float fRate);

	inline void setRate(float rate) { _rate = rate; }
	inline float getRate() const { return _rate; }

protected:
	float _rate;
};

/**
 @class EaseIn
 @brief EaseIn action with a rate.
 @details The timeline of inner action will be changed by:
         \f${ time }^{ rate }\f$.
 @ingroup Actions
 */
class EaseIn : public EaseRateAction {
public:
	virtual ~EaseIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseOut
 @brief EaseOut action with a rate.
 @details The timeline of inner action will be changed by:
         \f${ time }^ { (1/rate) }\f$.
 @ingroup Actions
 */
class EaseOut: public EaseRateAction {
public:
	virtual ~EaseOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseInOut
 @brief EaseInOut action with a rate
 @details If time * 2 < 1, the timeline of inner action will be changed by:
		 \f$0.5*{ time }^{ rate }\f$.
		 Else, the timeline of inner action will be changed by:
		 \f$1.0-0.5*{ 2-time }^{ rate }\f$.
 @ingroup Actions
 */
class EaseInOut: public EaseRateAction {
public:
	virtual ~EaseInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialIn
 @brief Ease Exponential In action.
 @details The timeline of inner action will be changed by:
		 \f${ 2 }^{ 10*(time-1) }-1*0.001\f$.
 @ingroup Actions
 */
class EaseExponentialIn: public ActionEase {
public:
	virtual ~EaseExponentialIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialOut
 @brief Ease Exponential Out
 @details The timeline of inner action will be changed by:
		 \f$1-{ 2 }^{ -10*(time-1) }\f$.
 @ingroup Actions
 */
class EaseExponentialOut : public ActionEase {
public:
	virtual ~EaseExponentialOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialInOut
 @brief Ease Exponential InOut
 @details If time * 2 < 1, the timeline of inner action will be changed by:
		 \f$0.5*{ 2 }^{ 10*(time-1) }\f$.
		 else, the timeline of inner action will be changed by:
		 \f$0.5*(2-{ 2 }^{ -10*(time-1) })\f$.
 @ingroup Actions
 */
class EaseExponentialInOut : public ActionEase {
public:
	virtual ~EaseExponentialInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineIn
 @brief Ease Sine In
 @details The timeline of inner action will be changed by:
		 \f$1-cos(time*\frac { \pi  }{ 2 } )\f$.
 @ingroup Actions
 */
class EaseSineIn : public ActionEase {
public:
	virtual ~EaseSineIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineOut
 @brief Ease Sine Out
 @details The timeline of inner action will be changed by:
		 \f$sin(time*\frac { \pi  }{ 2 } )\f$.
 @ingroup Actions
 */
class EaseSineOut : public ActionEase {
public:
	virtual ~EaseSineOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineInOut
 @brief Ease Sine InOut
 @details The timeline of inner action will be changed by:
		 \f$-0.5*(cos(\pi *time)-1)\f$.
 @ingroup Actions
 */
class EaseSineInOut : public ActionEase {
public:
	virtual ~EaseSineInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseElastic
 @brief Ease Elastic abstract class
 @since v0.8.2
 @ingroup Actions
 */
class EaseElastic : public ActionEase {
public:
	virtual ~EaseElastic() { }

	bool init(ActionInterval *action, float period = 0.3f);

	inline float getPeriod() const { return _period; }
	inline void setPeriod(float fPeriod) { _period = fPeriod; }

protected:
	float _period = 0.3f;
};

/**
 @class EaseElasticIn
 @brief Ease Elastic In action.
 @details If time == 0 or time == 1, the timeline of inner action will not be changed.
		 Else, the timeline of inner action will be changed by:
		 \f$-{ 2 }^{ 10*(time-1) }*sin((time-1-\frac { period }{ 4 } )*\pi *2/period)\f$.

 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseElasticIn : public EaseElastic {
public:
	virtual ~EaseElasticIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseElasticOut
 @brief Ease Elastic Out action.
 @details If time == 0 or time == 1, the timeline of inner action will not be changed.
		 Else, the timeline of inner action will be changed by:
		 \f${ 2 }^{ -10*time }*sin((time-\frac { period }{ 4 } )*\pi *2/period)+1\f$.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseElasticOut : public EaseElastic {
public:
	virtual ~EaseElasticOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseElasticInOut
 @brief Ease Elastic InOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseElasticInOut : public EaseElastic {
public:
	virtual ~EaseElasticInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceIn
 @brief EaseBounceIn action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
*/
class EaseBounceIn : public ActionEase {
public:
	virtual ~EaseBounceIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceOut
 @brief EaseBounceOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseBounceOut : public ActionEase {
public:
	virtual ~EaseBounceOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceInOut
 @brief EaseBounceInOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseBounceInOut : public ActionEase {
public:
	virtual ~EaseBounceInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackIn
 @brief EaseBackIn action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseBackIn : public ActionEase {
public:
	virtual ~EaseBackIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackOut
 @brief EaseBackOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseBackOut : public ActionEase {
public:
	virtual ~EaseBackOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackInOut
 @brief EaseBackInOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class EaseBackInOut : public ActionEase {
public:
	virtual ~EaseBackInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseBezierAction
@brief Ease Bezier
@ingroup Actions
*/
class EaseBezierAction : public ActionEase {
public:
	virtual ~EaseBezierAction() { }

	bool init(ActionInterval *, float p0, float p1, float p2, float p3);

	virtual void update(float time) override;

protected:
	float _p0;
	float _p1;
	float _p2;
	float _p3;
};

/**
@class EaseQuadraticActionIn
@brief Ease Quadratic In
@ingroup Actions
*/
class EaseQuadraticActionIn : public ActionEase {
public:
	virtual ~EaseQuadraticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuadraticActionOut
@brief Ease Quadratic Out
@ingroup Actions
*/
class EaseQuadraticActionOut : public ActionEase {
public:
	virtual ~EaseQuadraticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuadraticActionInOut
@brief Ease Quadratic InOut
@ingroup Actions
*/
class EaseQuadraticActionInOut : public ActionEase {
public:
	virtual ~EaseQuadraticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionIn
@brief Ease Quartic In
@ingroup Actions
*/
class EaseQuarticActionIn : public ActionEase {
public:
	virtual ~EaseQuarticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionOut
@brief Ease Quartic Out
@ingroup Actions
*/
class EaseQuarticActionOut : public ActionEase {
public:
	virtual ~EaseQuarticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionInOut
@brief Ease Quartic InOut
@ingroup Actions
*/
class EaseQuarticActionInOut : public ActionEase {
public:
	virtual ~EaseQuarticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionIn
@brief Ease Quintic In
@ingroup Actions
*/
class EaseQuinticActionIn : public ActionEase {
public:
	virtual ~EaseQuinticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionOut
@brief Ease Quintic Out
@ingroup Actions
*/
class EaseQuinticActionOut : public ActionEase {
public:
	virtual ~EaseQuinticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionInOut
@brief Ease Quintic InOut
@ingroup Actions
*/
class EaseQuinticActionInOut : public ActionEase {
public:
	virtual ~EaseQuinticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionIn
@brief Ease Circle In
@ingroup Actions
*/
class EaseCircleActionIn : public ActionEase {
public:
	virtual ~EaseCircleActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionOut
@brief Ease Circle Out
@ingroup Actions
*/
class EaseCircleActionOut : public ActionEase {
public:
	virtual ~EaseCircleActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionInOut
@brief Ease Circle InOut
@ingroup Actions
*/
class EaseCircleActionInOut : public ActionEase {
public:
	virtual ~EaseCircleActionInOut() {}

	virtual void update(float time) override;
};

/**
@class EaseCubicActionIn
@brief Ease Cubic In
@ingroup Actions
*/
class EaseCubicActionIn : public ActionEase {
public:
	virtual ~EaseCubicActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseCubicActionOut
@brief Ease Cubic Out
@ingroup Actions
*/
class EaseCubicActionOut : public ActionEase {
public:
	virtual ~EaseCubicActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCubicActionInOut
@brief Ease Cubic InOut
@ingroup Actions
*/
class EaseCubicActionInOut : public ActionEase {
public:
	virtual ~EaseCubicActionInOut() { }

	virtual void update(float time) override;
};

}

#endif /* XENOLITH_FEATURES_ACTIONS_XLACTIONEASE_H_ */
