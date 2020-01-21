//
// Created by ezra on 12/6/17.
//
#include <cmath>
#include <limits>


#include "softcut/Interpolate.h"
#include "softcut/Resampler.h"

#include "softcut/ReadWriteHead.h"

using namespace softcut;
using namespace std;

void ReadWriteHead::init(FadeCurves *fc) {
    start = 0.f;
    end = 0.f;
    active = 0;
    rate = 1.f;
    setFadeTime(0.1f);
    testBuf.init();
    queuedCrossfade = 0;
    queuedCrossfadeFlag = false;
    head[0].init(fc);
    head[1].init(fc);
}

void ReadWriteHead::processSample(sample_t in, sample_t *out) {

    *out = mixFade(head[0].peek(), head[1].peek(), head[0].fade(), head[1].fade());

    int numFades = (head[0].state_ == FadeIn || head[0].state_ == FadeOut)
            + (head[1].state_ == FadeIn || head[1].state_ == FadeOut);

    BOOST_ASSERT_MSG(!(head[0].state_ == Active && head[1].state_ == Active), "multiple active heads");

    head[0].poke(in, pre, rec, numFades);
    head[1].poke(in, pre, rec, numFades);

    takeAction(head[0].updatePhase(start, end, loopFlag));
    takeAction(head[1].updatePhase(start, end, loopFlag));

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);
    dequeueCrossfade();
}


void ReadWriteHead::processSampleNoRead(sample_t in, sample_t *out) {
    (void)out;
    int numFades = (head[0].state_ == FadeIn || head[0].state_ == FadeOut)
                   + (head[1].state_ == FadeIn || head[1].state_ == FadeOut);

    BOOST_ASSERT_MSG(!(head[0].state_ == Active && head[1].state_ == Active), "multiple active heads");

    head[0].poke(in, pre, rec, numFades);
    head[1].poke(in, pre, rec, numFades);

    takeAction(head[0].updatePhase(start, end, loopFlag));
    takeAction(head[1].updatePhase(start, end, loopFlag));

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);
    dequeueCrossfade();
}

void ReadWriteHead::processSampleNoWrite(sample_t in, sample_t *out) {
    (void)in;
    *out = mixFade(head[0].peek(), head[1].peek(), head[0].fade(), head[1].fade());

    BOOST_ASSERT_MSG(!(head[0].state_ == Active && head[1].state_ == Active), "multiple active heads");

    takeAction(head[0].updatePhase(start, end, loopFlag));
    takeAction(head[1].updatePhase(start, end, loopFlag));

    head[0].updateFade(fadeInc);
    head[1].updateFade(fadeInc);
    dequeueCrossfade();
}


void ReadWriteHead::setRate(rate_t x)
{
    rate = x;
    calcFadeInc();
    head[0].setRate(x);
    head[1].setRate(x);
}

void ReadWriteHead::setLoopStartSeconds(float x)
{
    start = x * sr;
}

void ReadWriteHead::setLoopEndSeconds(float x)
{
    end = x * sr;
}

void ReadWriteHead::takeAction(Action act)
{
    switch (act) {
        case Action::LoopPos:
            enqueueCrossfade(start);
            break;
        case Action::LoopNeg:
            enqueueCrossfade(end);
            break;
        case Action::Stop:
            break;
        case Action::None:
        default: ;;
    }
}

void ReadWriteHead::enqueueCrossfade(phase_t pos) {
    // std::cout <<"enqueuing crossfade\n";
    queuedCrossfade = pos;
    queuedCrossfadeFlag = true;
}

void ReadWriteHead::dequeueCrossfade() {
    State s = head[active].state();
    if(! (s == State::FadeIn || s == State::FadeOut)) {
	if(queuedCrossfadeFlag ) {
	    // std::cout <<"dequeuing crossfade\n";
	    cutToPhase(queuedCrossfade);
	}
	queuedCrossfadeFlag = false;
    }
}


void ReadWriteHead::cutToPhase(phase_t pos) {
    State s = head[active].state();

    if(s == State::FadeIn || s == State::FadeOut) {
	// should never enter this condition
	// cout << "bleeeeaaaaaaaaaaaaargh!!!!\n";
	return;
    }

    // activate the inactive head
    int newActive = active ^ 1;
    if(s != State::Inactive) {
        head[active].setState(State::FadeOut);
    }

    head[newActive].setState(State::FadeIn);
    head[newActive].setPhase(pos);

    head[active].active_ = false;
    head[newActive].active_ = true;
    active = newActive;
}

void ReadWriteHead::setFadeTime(float secs) {
    fadeTime = secs;
    calcFadeInc();
}
void ReadWriteHead::calcFadeInc() {
    fadeInc = (float) fabs(rate) / std::max(1.f, (fadeTime * sr));
    fadeInc = std::max(0.f, std::min(fadeInc, 1.f));
}

void ReadWriteHead::setBuffer(float *b, uint32_t bf) {
    buf = b;
    head[0].setBuffer(b, bf);
    head[1].setBuffer(b, bf);
}

void ReadWriteHead::setLoopFlag(bool val) {
    loopFlag = val;
}

void ReadWriteHead::setSampleRate(float sr_) {
    sr = sr_;
    head[0].setSampleRate(sr);
    head[1].setSampleRate(sr);
}

sample_t ReadWriteHead::mixFade(sample_t x, sample_t y, float a, float b) {
        return x * sinf(a * (float)M_PI_2) + y * sinf(b * (float) M_PI_2);
}

void ReadWriteHead::setRec(float x) {
    rec = x;
}

void ReadWriteHead::setPre(float x) {
    pre = x;
}

phase_t ReadWriteHead::getActivePhase() {
  return head[active].phase();
}

void ReadWriteHead::cutToPos(float seconds) {
    enqueueCrossfade(seconds * sr);
}

rate_t ReadWriteHead::getRate() {
    return rate;
}

void ReadWriteHead::setRecOffsetSamples(int d) {
    head[0].setRecOffsetSamples(d);
    head[1].setRecOffsetSamples(d);
}