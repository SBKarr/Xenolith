/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLActionEase.h"

#ifndef M_PI_X_2
#define M_PI_X_2 ((float)M_PI * 2.0f)
#endif

namespace stappler::xenolith::interpolation {

float tweenTo(float time, Type type, float *easingParam) {
	float delta = 0;

	switch (type) {
	case Linear: delta = linear(time); break;
	case Sine_EaseIn: delta = sineEaseIn(time); break;
	case Sine_EaseOut: delta = sineEaseOut(time); break;
	case Sine_EaseInOut: delta = sineEaseInOut(time); break;
	case Quad_EaseIn: delta = quadEaseIn(time); break;
	case Quad_EaseOut: delta = quadEaseOut(time); break;
	case Quad_EaseInOut: delta = quadEaseInOut(time); break;

	case Cubic_EaseIn: delta = cubicEaseIn(time); break;
	case Cubic_EaseOut: delta = cubicEaseOut(time); break;
	case Cubic_EaseInOut: delta = cubicEaseInOut(time); break;

	case Quart_EaseIn: delta = quartEaseIn(time); break;
	case Quart_EaseOut: delta = quartEaseOut(time); break;
	case Quart_EaseInOut: delta = quartEaseInOut(time); break;

	case Quint_EaseIn: delta = quintEaseIn(time); break;
	case Quint_EaseOut: delta = quintEaseOut(time); break;
	case Quint_EaseInOut: delta = quintEaseInOut(time); break;

	case Expo_EaseIn: delta = expoEaseIn(time); break;
	case Expo_EaseOut: delta = expoEaseOut(time); break;
	case Expo_EaseInOut: delta = expoEaseInOut(time); break;

	case Circ_EaseIn: delta = circEaseIn(time); break;
	case Circ_EaseOut: delta = circEaseOut(time); break;
	case Circ_EaseInOut: delta = circEaseInOut(time);break;

	case Elastic_EaseIn: {
		float period = 0.3f;
		if (nullptr != easingParam) {
			period = easingParam[0];
		}
		delta = elasticEaseIn(time, period);
	}
		break;
	case Elastic_EaseOut: {
		float period = 0.3f;
		if (nullptr != easingParam) {
			period = easingParam[0];
		}
		delta = elasticEaseOut(time, period);
	}
		break;
	case Elastic_EaseInOut: {
		float period = 0.3f;
		if (nullptr != easingParam) {
			period = easingParam[0];
		}
		delta = elasticEaseInOut(time, period);
	}
		break;

	case Back_EaseIn: delta = backEaseIn(time); break;
	case Back_EaseOut: delta = backEaseOut(time); break;
	case Back_EaseInOut: delta = backEaseInOut(time); break;

	case Bounce_EaseIn: delta = bounceEaseIn(time); break;
	case Bounce_EaseOut: delta = bounceEaseOut(time); break;
	case Bounce_EaseInOut: delta = bounceEaseInOut(time); break;
	case Custom: delta = customEase(time, easingParam); break;
	default: delta = sineEaseInOut(time); break;
	}

	return delta;
}

// Linear
float linear(float time) {
	return time;
}

// Sine Ease
float sineEaseIn(float time) {
	return -1 * cosf(time * (float) M_PI_2) + 1;
}

float sineEaseOut(float time) {
	return sinf(time * (float) M_PI_2);
}

float sineEaseInOut(float time) {
	return -0.5f * (cosf((float) M_PI * time) - 1);
}

// Quad Ease
float quadEaseIn(float time) {
	return time * time;
}

float quadEaseOut(float time) {
	return -1 * time * (time - 2);
}

float quadEaseInOut(float time) {
	time = time * 2;
	if (time < 1)
		return 0.5f * time * time;
	--time;
	return -0.5f * (time * (time - 2) - 1);
}

// Cubic Ease
float cubicEaseIn(float time) {
	return time * time * time;
}
float cubicEaseOut(float time) {
	time -= 1;
	return (time * time * time + 1);
}
float cubicEaseInOut(float time) {
	time = time * 2;
	if (time < 1)
		return 0.5f * time * time * time;
	time -= 2;
	return 0.5f * (time * time * time + 2);
}

// Quart Ease
float quartEaseIn(float time) {
	return time * time * time * time;
}

float quartEaseOut(float time) {
	time -= 1;
	return -(time * time * time * time - 1);
}

float quartEaseInOut(float time) {
	time = time * 2;
	if (time < 1)
		return 0.5f * time * time * time * time;
	time -= 2;
	return -0.5f * (time * time * time * time - 2);
}

// Quint Ease
float quintEaseIn(float time) {
	return time * time * time * time * time;
}

float quintEaseOut(float time) {
	time -= 1;
	return (time * time * time * time * time + 1);
}

float quintEaseInOut(float time) {
	time = time * 2;
	if (time < 1)
		return 0.5f * time * time * time * time * time;
	time -= 2;
	return 0.5f * (time * time * time * time * time + 2);
}

// Expo Ease
float expoEaseIn(float time) {
	return time == 0 ? 0 : powf(2, 10 * (time / 1 - 1)) - 1 * 0.001f;
}
float expoEaseOut(float time) {
	return time == 1 ? 1 : (-powf(2, -10 * time / 1) + 1);
}
float expoEaseInOut(float time) {
	time /= 0.5f;
	if (time < 1) {
		time = 0.5f * powf(2, 10 * (time - 1));
	} else {
		time = 0.5f * (-powf(2, -10 * (time - 1)) + 2);
	}

	return time;
}

// Circ Ease
float circEaseIn(float time) {
	return -1 * (sqrt(1 - time * time) - 1);
}
float circEaseOut(float time) {
	time = time - 1;
	return sqrt(1 - time * time);
}
float circEaseInOut(float time) {
	time = time * 2;
	if (time < 1)
		return -0.5f * (sqrt(1 - time * time) - 1);
	time -= 2;
	return 0.5f * (sqrt(1 - time * time) + 1);
}

// Elastic Ease
float elasticEaseIn(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		float s = period / 4;
		time = time - 1;
		newT = -powf(2, 10 * time) * sinf((time - s) * M_PI_X_2 / period);
	}

	return newT;
}
float elasticEaseOut(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		float s = period / 4;
		newT = powf(2, -10 * time) * sinf((time - s) * M_PI_X_2 / period) + 1;
	}

	return newT;
}
float elasticEaseInOut(float time, float period) {

	float newT = 0;
	if (time == 0 || time == 1) {
		newT = time;
	} else {
		time = time * 2;
		if (!period) {
			period = 0.3f * 1.5f;
		}

		float s = period / 4;

		time = time - 1;
		if (time < 0) {
			newT = -0.5f * powf(2, 10 * time) * sinf((time - s) * M_PI_X_2 / period);
		} else {
			newT = powf(2, -10 * time) * sinf((time - s) * M_PI_X_2 / period) * 0.5f + 1;
		}
	}
	return newT;
}

// Back Ease
float backEaseIn(float time) {
	float overshoot = 1.70158f;
	return time * time * ((overshoot + 1) * time - overshoot);
}
float backEaseOut(float time) {
	float overshoot = 1.70158f;

	time = time - 1;
	return time * time * ((overshoot + 1) * time + overshoot) + 1;
}
float backEaseInOut(float time) {
	float overshoot = 1.70158f * 1.525f;

	time = time * 2;
	if (time < 1) {
		return (time * time * ((overshoot + 1) * time - overshoot)) / 2;
	} else {
		time = time - 2;
		return (time * time * ((overshoot + 1) * time + overshoot)) / 2 + 1;
	}
}

// Bounce Ease
float bounceTime(float time) {
	if (time < 1 / 2.75) {
		return 7.5625f * time * time;
	} else if (time < 2 / 2.75) {
		time -= 1.5f / 2.75f;
		return 7.5625f * time * time + 0.75f;
	} else if (time < 2.5 / 2.75) {
		time -= 2.25f / 2.75f;
		return 7.5625f * time * time + 0.9375f;
	}

	time -= 2.625f / 2.75f;
	return 7.5625f * time * time + 0.984375f;
}
float bounceEaseIn(float time) {
	return 1 - bounceTime(1 - time);
}

float bounceEaseOut(float time) {
	return bounceTime(time);
}

float bounceEaseInOut(float time) {
	float newT = 0;
	if (time < 0.5f) {
		time = time * 2;
		newT = (1 - bounceTime(1 - time)) * 0.5f;
	} else {
		newT = bounceTime(time * 2 - 1) * 0.5f + 0.5f;
	}

	return newT;
}

// Custom Ease
float customEase(float time, float *easingParam) {
	if (easingParam) {
		float tt = 1 - time;
		return easingParam[1] * tt * tt * tt + 3 * easingParam[3] * time * tt * tt + 3 * easingParam[5] * time * time * tt + easingParam[7] * time * time * time;
	}
	return time;
}

float easeIn(float time, float rate) {
	return powf(time, rate);
}

float easeOut(float time, float rate) {
	return powf(time, 1 / rate);
}

float easeInOut(float time, float rate) {
	time *= 2;
	if (time < 1) {
		return 0.5f * powf(time, rate);
	} else {
		return (1.0f - 0.5f * powf(2 - time, rate));
	}
}

float quadraticIn(float time) {
	return powf(time, 2);
}

float quadraticOut(float time) {
	return -time * (time - 2);
}

float quadraticInOut(float time) {

	float resultTime = time;
	time = time * 2;
	if (time < 1) {
		resultTime = time * time * 0.5f;
	} else {
		--time;
		resultTime = -0.5f * (time * (time - 2) - 1);
	}
	return resultTime;
}

static float evaluateCubic(float t, float p1, float p2) {
	return
		// std::pow(1.0f - t, 3.0f) * p0 // - p0 = 0.0f
		3.0f * std::pow(1.0f - t, 2) * t * p1
		+ 3.0f * (1.0f - t) * std::pow(t, 2.0f) * p2
		+ std::pow(t, 3.0f); // p3 = 1.0f
}

static constexpr float BezieratErrorBound = 0.001f;

static float truncateBorders(float t) {
	if (std::fabs(t) < BezieratErrorBound) {
		return 0.0f;
	} else if (std::fabs(t - 1.0f) < BezieratErrorBound) {
		return 1.0f;
	}
	return t;
}

float bezieratFunction(float t, float x1, float y1, float x2, float y2) {
    float start = 0.0f;
    float end = 1.0f;

    uint32_t iter = 0;

    while (true) {
    	const float midpoint = (start + end) / 2;
    	const float estimate = evaluateCubic(midpoint, x1, x2);
		if (std::abs(t - estimate) < BezieratErrorBound) {
			return truncateBorders(evaluateCubic(midpoint, y1, y2));
		}
		if (estimate < t) {
			start = midpoint;
		} else {
			end = midpoint;
		}
		++ iter;
    }
    return nan();
}

}

namespace stappler::xenolith {

ActionEase::~ActionEase() { }

bool ActionEase::init(ActionInterval *action) {
	XLASSERT(action != nullptr, "");

	if (ActionInterval::init(action->getDuration())) {
		_inner = action;
		return true;
	}

	return false;
}


void ActionEase::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	_inner->startWithTarget(_target);
}

void ActionEase::stop(void) {
	_inner->stop();
	ActionInterval::stop();
}

void ActionEase::update(float time) {
	_inner->update(time);
}

EaseRateAction::~EaseRateAction() { }

bool EaseRateAction::init(ActionInterval *action, float rate) {
	if (ActionEase::init(action)) {
		_rate = rate;
		return true;
	}

	return false;
}

void EaseIn::update(float time) {
	_inner->update(interpolation::easeIn(time, _rate));
}

void EaseOut::update(float time) {
	_inner->update(interpolation::easeOut(time, _rate));
}

void EaseInOut::update(float time) {
	_inner->update(interpolation::easeInOut(time, _rate));
}

void EaseExponentialIn::update(float time) {
	_inner->update(interpolation::expoEaseIn(time));
}

void EaseExponentialOut::update(float time) {
	_inner->update(interpolation::expoEaseOut(time));
}

void EaseExponentialInOut::update(float time) {
	_inner->update(interpolation::expoEaseInOut(time));
}

void EaseSineIn::update(float time) {
	_inner->update(interpolation::sineEaseIn(time));
}

void EaseSineOut::update(float time) {
	_inner->update(interpolation::sineEaseOut(time));
}

void EaseSineInOut::update(float time) {
	_inner->update(interpolation::sineEaseInOut(time));
}

bool EaseElastic::init(ActionInterval *action, float period) {
	if (ActionEase::init(action)) {
		_period = period;
		return true;
	}

	return false;
}

void EaseElasticIn::update(float time) {
	_inner->update(interpolation::elasticEaseIn(time, _period));
}

void EaseElasticOut::update(float time) {
	_inner->update(interpolation::elasticEaseOut(time, _period));
}

void EaseElasticInOut::update(float time) {
	_inner->update(interpolation::elasticEaseInOut(time, _period));
}

void EaseBounceIn::update(float time) {
	_inner->update(interpolation::bounceEaseIn(time));
}

void EaseBounceOut::update(float time) {
	_inner->update(interpolation::bounceEaseOut(time));
}

void EaseBounceInOut::update(float time) {
	_inner->update(interpolation::bounceEaseInOut(time));
}

void EaseBackIn::update(float time) {
	_inner->update(interpolation::backEaseIn(time));
}

void EaseBackOut::update(float time) {
	_inner->update(interpolation::backEaseOut(time));
}

void EaseBackInOut::update(float time) {
	_inner->update(interpolation::backEaseInOut(time));
}

bool EaseBezierAction::init(ActionInterval *action, float p0, float p1, float p2, float p3) {
	if (ActionEase::init(action)) {
		_p0 = p0;
		_p1 = p1;
		_p2 = p2;
		_p3 = p3;
		return true;
	}
	return false;
}

void EaseBezierAction::update(float time) {
	_inner->update(interpolation::bezieratFunction(time, _p0, _p1, _p2, _p3));
}

void EaseQuadraticActionIn::update(float time) {
	_inner->update(interpolation::quadraticIn(time));
}

void EaseQuadraticActionOut::update(float time) {
	_inner->update(interpolation::quadraticOut(time));
}

void EaseQuadraticActionInOut::update(float time) {
	_inner->update(interpolation::quadraticInOut(time));
}

void EaseQuarticActionIn::update(float time) {
	_inner->update(interpolation::quartEaseIn(time));
}

void EaseQuarticActionOut::update(float time) {
	_inner->update(interpolation::quartEaseOut(time));
}

void EaseQuarticActionInOut::update(float time) {
	_inner->update(interpolation::quartEaseInOut(time));
}

void EaseQuinticActionIn::update(float time) {
	_inner->update(interpolation::quintEaseIn(time));
}

void EaseQuinticActionOut::update(float time) {
	_inner->update(interpolation::quintEaseOut(time));
}

void EaseQuinticActionInOut::update(float time) {
	_inner->update(interpolation::quintEaseInOut(time));
}

void EaseCircleActionIn::update(float time) {
	_inner->update(interpolation::circEaseIn(time));
}

void EaseCircleActionOut::update(float time) {
	_inner->update(interpolation::circEaseOut(time));
}

void EaseCircleActionInOut::update(float time) {
	_inner->update(interpolation::circEaseInOut(time));
}

void EaseCubicActionIn::update(float time) {
	_inner->update(interpolation::cubicEaseIn(time));
}

void EaseCubicActionOut::update(float time) {
	_inner->update(interpolation::cubicEaseOut(time));
}

void EaseCubicActionInOut::update(float time) {
	_inner->update(interpolation::cubicEaseInOut(time));
}

}
