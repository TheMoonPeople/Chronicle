#pragma once

#include <memory>
#include <optional>
#include <string>

#include "fishing/framework.hpp"

namespace dc::fishing {

/**
 * The steps of a session, numbered as ED_FISHING_* (ebattle.hpp), which the game's code reads.
 */
enum class Step {
    Init = 0,
    Stand = 1,
    CastStart = 2,
    Casting = 3,
    WaitBite = 4,
    ReelIn = 5,
    BiteCheck = 6,
    Hooked = 7,
    Landed = 8,
    Sink = 9,
    Battle = 10,
    Result = 11,
    Finish = 12,
};

enum class Sound {
    Cast,    /**< At the angler. */
    Sink,    /**< At the float. */
    Bite,    /**< The fish coming out of the water. */
    Landed,  /**< The fish put away. */
    Fanfare, /**< The catch held up. */
    Warning, /**< The line close to snapping. */
    Snap,    /**< The line breaking. */
};

/**
 * What a session reads each tick.
 */
struct Input {
    bool  cross = false;      /**< Pressed this tick. */
    bool  cross_held = false;
    bool  circle = false;
    bool  square = false;
    float stick_x = 0.0f; /**< The left stick, -1 to 1. */
    float stick_y = 0.0f; /**< -1 (down) to 1 (up). */
    int   fish_status = -1; /**< FishingFishStatus: -1, 7 nibbling, 8 on the hook, 11 fighting. */
    int   fish_no = -1;     /**< The fish FishingFishStatus last looked at. */
    bool  debug = false;    /**< Debug mode, which sets the hook itself. */
};

/**
 * The game's side of a session: the angler, the line and the fish, the camera, the sounds and the
 * messages. fishing/angler.cpp is the game's; tests have their own.
 */
class Angler {
public:
    virtual ~Angler() = default;

    virtual void Motion(int motion, int flags) = 0;
    virtual int  CurrentMotion() = 0;
    virtual bool MotionDone() = 0;
    virtual void HoldStill() = 0; /**< Keeps the angler from walking this tick. */
    virtual void Turn(float stick) = 0;
    virtual void Play(Sound sound) = 0;
    virtual void Rumble(int intensity, int ticks) = 0;
    virtual void Ripple(float radius) = 0; /**< Rings on the water at the float. */
    virtual void PullFloat(float tension) = 0;
    virtual void BiteCue() = 0;
    virtual void Help(bool waiting) = 0; /**< The help line: standing, or waiting for a bite. */
    virtual void OpenMenu(int event) = 0;
    virtual void FollowLine() = 0;
    virtual void CatchCamera(float distance) = 0;
    virtual void FadeMusic(bool out) = 0;
    virtual bool FloatCaught() = 0; /**< The float snagged on something. */
    virtual bool HookLow() = 0;     /**< The hook near the water. */

    virtual void LoseBait() = 0; /**< Says so and takes it off the hook, unless it is a lure. */
    virtual void UseBait() = 0;  /**< Off the hook as the fish takes it: eaten, or a lure back to the bag. */
    virtual void ResetFish() = 0;
    virtual void RestartBite() = 0; /**< The bite stage's cast starts over: the float settled, or the angler moved. */
    virtual void WatchFish(int fish_no) = 0;
    virtual void HookFish(int fish_no) = 0; /**< Sets the hook and starts loading the fish's model. */
    virtual float HookedSize() = 0;          /**< The hooked fish's size, as CFish keeps it. */
    virtual float FishDistance() = 0;        /**< The hooked fish (or the float) from the angler, across the water. */
    virtual void  ReleaseFish() = 0;         /**< The hooked fish swims free: the line broke. */
    virtual void  GrowHooked(float factor) = 0; /**< The hooked fish's size times factor: a rod's Heft. */
    virtual item::Rod RodInUse() = 0;
    virtual bool ModelReady() = 0;
    virtual void LandFish() = 0;
    virtual void PutAwayFish() = 0;
    virtual std::optional<Catch> Caught() = 0;
    virtual bool                 MessageDone() = 0;
    virtual const Species       *FishSpecies(int fish_no) = 0;
    virtual const Species       *NibblingSpecies() = 0; /**< FishingFishStatus's fish_no isn't the nibbling fish's. */
    virtual TimeOfDay            Time() = 0;

    virtual const Place   &CurrentPlace() = 0;
    virtual BiteContext    Bait() = 0;
    virtual OutcomeContext Outcome() = 0;
    virtual double         Random() = 0;
};

/**
 * One angler's fishing, a tick at a time: retail's EdMoveChara block, asking the place's stages.
 */
class Session {
public:
    void Reset();
    void Tick(Angler &angler, const Input &input);

    [[nodiscard]] Step  CurrentStep() const { return step_; }
    [[nodiscard]] const Fight *CurrentFight() const { return fight_.get(); }
    [[nodiscard]] int          FightTicks() const { return step_ == Step::Battle ? ticks_ : 0; }
    [[nodiscard]] float FloatWeight() const { return float_weight_; } /**< How hard the float is held to the rod; below 0, free. */
    [[nodiscard]] float HookWeight() const { return hook_weight_; }

private:
    void Hook(Angler &angler, const Input &input);

    enum class Message {
        Evaluate,
        Wait,
        Done,
    };

    Step    step_ = Step::Init;
    Message message_ = Message::Evaluate;
    int     ticks_ = 0; /**< In the step. */
    int     wait_ = 0;  /**< Waiting for a bite, for the idle motions. */
    const Species         *hooked_ = nullptr; /**< The fish on the hook. */
    std::unique_ptr<Fight> fight_;            /**< The hooked fish's fight, from the place's fight stage. */
    std::string    bait_;             /**< The bait it took, for the catch. */
    bool           held_cross_ = false; /**< Cross pressed on the bite's tick, for the Hooked step. */
    float   float_weight_ = -1.0f;
    float   hook_weight_ = -1.0f;
};

} // namespace dc::fishing
