#include "fishing/session.hpp"

#include <algorithm>
#include <utility>

namespace dc::fishing {

void Session::Reset() {
    fight_.reset();
    step_ = Step::Init;
    message_ = Message::Evaluate;
    ticks_ = 0;
    wait_ = 0;
    held_cross_ = false;
}

// The bite: which fish, and the bait it took, kept for the catch or the loss. Cross pressed on the
// bite's own tick sets the hook on the next, rather than counting as early or as reeling in.
void Session::Hook(Angler &angler, const Input &input) {
    step_ = Step::Hooked;
    hooked_ = angler.FishSpecies(input.fish_no);
    bait_ = std::string(angler.Bait().bait);
    held_cross_ = input.cross;
    angler.BiteCue();
}

void Session::Tick(Angler &angler, const Input &input) {
    const Place &place = angler.CurrentPlace();
    bool         follow = false;
    float_weight_ = -1.0f;
    hook_weight_ = -1.0f;

    switch (step_) {
        case Step::Init:
            step_ = Step::Stand;
            angler.ResetFish();
            angler.WatchFish(-1);
            wait_ = 0;
            ticks_ = 0;
            break;
        case Step::Stand:
            if (ticks_ > 120) {
                angler.Help(false);
            }
            angler.WatchFish(-1);
            if (input.cross) {
                step_ = Step::CastStart;
                ticks_ = 0;
            }
            if (input.circle) {
                angler.OpenMenu(0x85);
                angler.Motion(0, 0);
            }
            if (input.square) {
                angler.OpenMenu(0x86);
                angler.Motion(0, 0);
            }
            float_weight_ = 1.0f;
            break;
        case Step::CastStart:
            angler.Motion(4, 6);
            step_ = Step::Casting;
            ticks_ = 0;
            break;
        case Step::Casting:
            angler.HoldStill();
            if (ticks_ == 90) {
                angler.Play(Sound::Cast);
            }
            if (angler.MotionDone()) {
                angler.Motion(5, 0);
                step_ = Step::WaitBite;
                ticks_ = 0;
                wait_ = 0;
                angler.RestartBite();
            }
            break;
        case Step::WaitBite:
            angler.Help(true);
            angler.HoldStill();
            // Idle motions while nothing bites: holding the rod, then looking about, then a stretch.
            if (wait_ < 600) {
                angler.Motion(5, 0);
            } else if (wait_ < 1200) {
                angler.Motion(14, 0);
            } else {
                if (wait_ == 1200) {
                    angler.Motion(15, 6);
                }
                if (wait_ > 1250 && angler.MotionDone()) {
                    angler.Motion(5, 4);
                    wait_ = 0;
                }
            }
            angler.Turn(input.stick_x);
            if (input.stick_x != 0.0f) {
                wait_ = 0;
                angler.Motion(13, 0);
                angler.RestartBite();
            }
            if (input.fish_status == 8) {
                Hook(angler, input);
            } else if (input.cross || (ticks_ > 30 && angler.FloatCaught())) {
                step_ = Step::ReelIn;
                angler.Motion(7, 6);
                ticks_ = 0;
                angler.RestartBite();
            } else if (input.fish_status == 7) {
                step_ = Step::BiteCheck;
            }
            wait_++;
            follow = true;
            break;
        case Step::ReelIn:
            if (ticks_ == 40 && angler.HookLow()) {
                angler.Play(Sound::Sink);
            }
            if (angler.MotionDone()) {
                angler.Motion(0, 0);
                step_ = Step::Stand;
                wait_ = 0;
            }
            if (ticks_ > 30) {
                float_weight_ = 0.02f * static_cast<float>(ticks_ - 30);
            }
            float_weight_ = std::min(float_weight_, 1.0f);
            break;
        case Step::BiteCheck: {
            angler.Help(true);
            if (input.fish_status == 8) {
                Hook(angler, input);
                follow = true;
                break;
            }
            if (input.fish_status != 7) {
                step_ = Step::WaitBite;
                ticks_ = 0;
                wait_ = 0;
            }
            if (input.cross) {
                step_ = Step::Sink;
                angler.Motion(6, 6);
                ticks_ = 0;
                angler.Play(Sound::Sink);
                const BiteContext bait = angler.Bait();
                const std::string bait_id(bait.bait);
                const bool        lost = place.bite->LosesBait(LossCause::PressedEarly, bait);
                if (lost) {
                    angler.LoseBait();
                }
                Publish(Lost{.species = angler.NibblingSpecies(), .place = &place, .bait = bait_id, .why = LossCause::PressedEarly, .bait_lost = lost});
            }
            // The nibble: the float bobs, now and then hard enough to ring the water and rumble.
            const float pull = -0.15f * static_cast<float>(angler.Random());
            if (pull < -0.1f) {
                angler.Rumble(80, 2);
                angler.Ripple(10.0f);
            }
            angler.PullFloat(pull);
            follow = true;
            break;
        }
        case Step::Hooked: {
            angler.Help(true);
            const bool pressed = input.cross || std::exchange(held_cross_, false);
            if (input.fish_status != 8) {
                step_ = Step::WaitBite;
                wait_ = 0;
                const bool lost = place.bite->LosesBait(LossCause::LetGo, angler.Bait());
                if (lost) {
                    angler.LoseBait();
                }
                Publish(Lost{.species = hooked_, .place = &place, .bait = bait_, .why = LossCause::LetGo, .bait_lost = lost});
                ticks_ = 0;
            } else {
                if (pressed || input.debug) {
                    angler.Play(Sound::Cast);
                    // Retail deleted the bait every tick of the fight; it is taken once, here unless
                    // the fight can be lost.
                    if (!place.fight->BaitStaysOn()) {
                        angler.UseBait();
                    }
                    step_ = Step::Battle;
                    angler.Motion(12, 2);
                    angler.HookFish(input.fish_no);
                    const item::Rod rod = angler.RodInUse();
                    if (place.fight->ReadsRods()) {
                        angler.GrowHooked(EffectOf(rod).size);
                    }
                    fight_ = place.fight->Begin(
                        Hooked{.species = hooked_, .size = angler.HookedSize(), .distance = angler.FishDistance(), .rod = rod});
                    ticks_ = 0;
                }
                const float tug = -(0.1f + 0.05f * static_cast<float>(angler.Random()));
                if (tug < -0.12f) {
                    angler.Rumble(100, 10);
                }
                angler.PullFloat(tug);
                follow = true;
            }
            break;
        }
        case Step::Battle: {
            const FightContext context{
                .ticks = ticks_,
                .input = {.reel = input.cross_held, .tapped = input.cross, .stick_x = input.stick_x, .stick_y = input.stick_y},
                .distance = angler.FishDistance(),
                .model_ready = [&] { return angler.ModelReady(); },
                .random = [&] { return angler.Random(); },
                .rumble = [&](int intensity, int ticks) { angler.Rumble(intensity, ticks); },
                .warn = [&] { angler.Play(Sound::Warning); }};
            switch (fight_ != nullptr ? fight_->Step(context) : FightResult::Landed) {
                case FightResult::Landed:
                    // Before the landed fish's model goes where the bait's is.
                    if (place.fight->BaitStaysOn()) {
                        angler.UseBait();
                    }
                    step_ = Step::Landed;
                    angler.Motion(6, 6);
                    angler.LandFish();
                    angler.Play(Sound::Bite);
                    fight_.reset();
                    break;
                case FightResult::Snapped: {
                    const bool lost = fight_->LosesBait(context);
                    if (lost) {
                        angler.LoseBait();
                    }
                    angler.Play(Sound::Snap);
                    angler.ReleaseFish();
                    Publish(Lost{.species = hooked_, .place = &place, .bait = bait_, .why = LossCause::Snapped, .bait_lost = lost});
                    step_ = Step::Sink;
                    angler.Motion(6, 6);
                    ticks_ = 0;
                    fight_.reset();
                    break;
                }
                case FightResult::Fighting:
                    break;
            }
            follow = true;
            break;
        }
        case Step::Landed:
            step_ = Step::Result;
            angler.Motion(10, 6);
            message_ = Message::Evaluate;
            follow = true;
            angler.FadeMusic(true);
            break;
        case Step::Sink:
            if (angler.MotionDone()) {
                step_ = Step::Stand;
                angler.Motion(0, 0);
                angler.WatchFish(input.fish_no);
                angler.ResetFish();
                wait_ = 0;
            }
            if (ticks_ > 40) {
                float_weight_ = 0.02f * static_cast<float>(ticks_ - 40);
            }
            float_weight_ = std::min(float_weight_, 1.0f);
            break;
        case Step::Result:
            angler.CatchCamera(40.0f);
            if (angler.CurrentMotion() == 10) {
                if (angler.MotionDone()) {
                    angler.Motion(8, 0);
                    angler.Play(Sound::Fanfare);
                }
            } else {
                switch (message_) {
                    case Message::Evaluate:
                        if (std::optional<Catch> fish = angler.Caught()) {
                            fish->place = &place;
                            fish->bait = bait_;
                            fish->time = angler.Time();
                            place.outcome->Apply(*fish, angler.Outcome());
                            Publish(*fish);
                            message_ = Message::Wait;
                        } else {
                            message_ = Message::Done;
                        }
                        break;
                    case Message::Wait:
                        if (angler.MessageDone()) {
                            message_ = Message::Done;
                        }
                        break;
                    case Message::Done:
                        angler.FadeMusic(false);
                        angler.Motion(0, 0);
                        step_ = Step::Finish;
                        angler.Motion(9, 6);
                        ticks_ = 0;
                        break;
                }
            }
            hook_weight_ = 1.0f;
            break;
        case Step::Finish:
            angler.CatchCamera(30.0f);
            if (ticks_ == 120) {
                angler.PutAwayFish();
                angler.Play(Sound::Landed);
            }
            if (angler.MotionDone()) {
                angler.Motion(0, 0);
                step_ = Step::Init;
                angler.PutAwayFish();
                angler.WatchFish(-1);
                angler.ResetFish();
            }
            hook_weight_ = 1.0f;
            float_weight_ = 1.0f;
            break;
    }

    if (follow) {
        angler.FollowLine();
    }
    ticks_++;
}

} // namespace dc::fishing
