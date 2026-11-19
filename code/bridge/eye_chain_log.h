#pragma once
#include <stdint.h>
#include <string>

// Bounded evidence of the SR/NR path, diagnostic only. It receives the outcome of each eye's present()
// and never changes history, presets or processing. After each transition (start, return to the world,
// theatre entry or exit) it reports:
//   EYE_CHAIN eye=E path=dlss_sr|dlss_nr sr= nr= transition=T generation=G     first success of each eye
//   EYE_CHAIN pair transition=T generation=G left_sr= left_nr= right_sr= right_nr=
//                                    both eyes succeeded in the same generation: one effective stereo pair
//   EYE_CHAIN eye=E path=bilinear reason=R transition=T generation=G           an eye fell back before the pair
//   EYE_CHAIN fallback_after_pair eye=E reason=R transition=T generation=G pair_generation=P
//                                    the first fallback after the pair of this transition
//   EYE_CHAIN window_closed transitions=64                                    a 65th transition came: from
//                                    here to the end of the session nothing more is reported or credited
// Each transition reports at most six lines (two first successes, two early fallbacks, the pair and one
// fallback after it). The session start is transition 0 and at most kTransitions more count; the next one
// closes the window with one line, so a presented eye is never credited to an earlier transition. A session
// reports at most kLines lines.
namespace gx_eye_chain_log
{
class PairLog
{
  public:
    enum { kTransitions = 64, kLinesPerTransition = 6, kLines = (kTransitions + 1) * kLinesPerTransition + 1 };

    template <class Say> void transition(Say say)
    {
        if (closed_)
            return;
        if (transitions_ < kTransitions)
        {
            ++transitions_;
            reset();
            return;
        }
        closed_ = true;
        reset();
        emit("EYE_CHAIN window_closed transitions=" + std::to_string(transitions_), say);
    }
    // A new session: the window opens again at transition 0.
    void close()
    {
        transitions_ = lines_ = 0;
        closed_ = false;
        reset();
    }
    unsigned transitions() const { return transitions_; }
    unsigned lines() const { return lines_; }
    bool paired() const { return paired_; }
    bool closed() const { return closed_; }

    // Eye e rendered generation g through SR and/or NR.
    template <class Say> void success(unsigned e, uint64_t g, bool sr, bool nr, Say say)
    {
        if (closed_ || e > 1 || !(sr || nr))
            return;
        if (!mask_ || g != generation_)
        {
            generation_ = g;
            mask_ = 0;
        }
        mask_ |= 1u << e;
        sr_[e] = sr;
        nr_[e] = nr;
        if (!(eyeLogged_ & (1u << e)))
        {
            eyeLogged_ |= 1u << e;
            emit("EYE_CHAIN eye=" + std::to_string(e) + " path=" + (sr ? "dlss_sr" : "dlss_nr") +
                     " sr=" + bit(sr) + " nr=" + bit(nr) + " transition=" + std::to_string(transitions_) +
                     " generation=" + std::to_string(g),
                 say);
        }
        if (mask_ == 3 && !paired_)
        {
            paired_ = true;
            pairGeneration_ = g;
            emit("EYE_CHAIN pair transition=" + std::to_string(transitions_) + " generation=" + std::to_string(g) +
                     " left_sr=" + bit(sr_[0]) + " left_nr=" + bit(nr_[0]) + " right_sr=" + bit(sr_[1]) +
                     " right_nr=" + bit(nr_[1]),
                 say);
        }
    }
    // Eye e of generation g took the bilinear path instead.
    template <class Say> void fallback(unsigned e, uint64_t g, const char* reason, Say say)
    {
        if (closed_ || e > 1)
            return;
        if (g == generation_)
            mask_ &= ~(1u << e);
        if (paired_)
        {
            if (!afterLogged_)
            {
                afterLogged_ = true;
                emit("EYE_CHAIN fallback_after_pair eye=" + std::to_string(e) + " reason=" + reason +
                         " transition=" + std::to_string(transitions_) + " generation=" + std::to_string(g) +
                         " pair_generation=" + std::to_string(pairGeneration_),
                     say);
            }
        }
        else if (!(fallbackLogged_ & (1u << e)))
        {
            fallbackLogged_ |= 1u << e;
            emit("EYE_CHAIN eye=" + std::to_string(e) + " path=bilinear reason=" + reason +
                     " transition=" + std::to_string(transitions_) + " generation=" + std::to_string(g),
                 say);
        }
    }

  private:
    unsigned transitions_ = 0, lines_ = 0, eyeLogged_ = 0, fallbackLogged_ = 0, mask_ = 0;
    uint64_t generation_ = 0, pairGeneration_ = 0;
    bool sr_[2] = {}, nr_[2] = {}, paired_ = false, afterLogged_ = false, closed_ = false;

    static const char* bit(bool v) { return v ? "1" : "0"; }
    void reset()
    {
        eyeLogged_ = fallbackLogged_ = mask_ = 0;
        generation_ = pairGeneration_ = 0;
        paired_ = afterLogged_ = false;
    }
    template <class Say> void emit(const std::string& line, Say say)
    {
        ++lines_;
        say(line);
    }
};
}
