#pragma once
/* The F7 archives from the title screen: the pure rules (no guest memory, no Win32), tested in
 * tests/test_map_tools.cpp; cv_test_tools.cpp's TEST_FRAME reads the guest and applies them.
 *
 *  - title_menu_ready(): the main menu (New Game / Load Game ...) is on screen and idle.
 *  - Machine: "Maps" from the title = start a new game by the menu's own branch, wait for control, then warp.
 *
 * Task bits are the ones of system+0x64 (active = +0x64 & ~+0x68), see docs/PLANO-IMPLEMENTACAO-v1.7.md. */
#include <cstdint>

namespace cv_title_flow {
// What the F7 window may do now (published by the game thread, read by the window).
enum Context { CTX_NONE = 0, CTX_GAMEPLAY = 1, CTX_TITLE = 2 };

constexpr uint32_t TASK_TITLE = 0x4;       // the title sequence (logos, intro movie, PRESS START)
constexpr uint32_t TASK_MENU = 0x8;        // the main menu
constexpr uint32_t TASK_SETUP = 0x10;      // new game / load setup
constexpr uint32_t TASK_ROOM = 0x80;       // the room, gameplay
constexpr uint32_t TASK_SAVE = 0x8000;     // the save / load screen
constexpr uint32_t FLAG74_FADE = 0x2;      // a fade is running

constexpr uint32_t active_tasks(uint32_t tasks, uint32_t blocked) { return tasks & ~blocked; }
// The title sequence plays its intro movie in states 7..9 (boot_skip.cpp); the main menu only exists past it.
constexpr bool title_state_past_intro(uint32_t title_state) { return !(title_state >= 7 && title_state <= 9); }
constexpr bool menu_or_title(uint32_t active) { return (active & (TASK_TITLE | TASK_MENU)) != 0; }

struct Snapshot {
    bool mapped = false;       // the system block is readable
    uint32_t tasks = 0, blocked = 0, flags74 = 0, title_state = 0;
};
// The main menu is the active task, nothing else is loading or running, no fade, and the title sequence is past the intro.
constexpr bool title_menu_ready(const Snapshot& s) {
    const uint32_t a = active_tasks(s.tasks, s.blocked);
    return s.mapped && (a & TASK_MENU) && !(a & (TASK_ROOM | TASK_SAVE | TASK_SETUP)) && !(s.flags74 & FLAG74_FADE)
        && title_state_past_intro(s.title_state);
}
constexpr int context_of(bool gameplay_ready, bool title_ready) {
    return gameplay_ready ? CTX_GAMEPLAY : title_ready ? CTX_TITLE : CTX_NONE;
}
// Which tabs of F7 work in a context: Load and Maps in game or at the title, Save only in game.
constexpr bool load_allowed(int ctx) { return ctx == CTX_GAMEPLAY || ctx == CTX_TITLE; }
constexpr bool maps_allowed(int ctx) { return ctx == CTX_GAMEPLAY || ctx == CTX_TITLE; }
constexpr bool save_allowed(int ctx) { return ctx == CTX_GAMEPLAY; }

// The writes of the main menu's "New Game" branch (return value 2: +0x50 &= ~0x200, +0x64 = 0x00300010), unproven
// until the CV_TITLE_PROBE capture of a natural New Game confirms it.
struct NewGameWrites { uint32_t flags50, tasks; };
constexpr NewGameWrites new_game_writes(uint32_t flags50) { return { flags50 & ~0x200u, 0x00300010u }; }

enum class Phase { IDLE, NEWGAME, WAIT_CONTROL, DONE };
enum class Step { NONE, START_NEW_GAME, WARP, ABORT_NOT_READY, ABORT_TIMEOUT, ABORT_MENU };
struct Inputs {
    bool title_ready = false;   // title_menu_ready now
    bool game_ready = false;    // active_game(): the character can be controlled
    uint32_t active = 0;        // active task bits now
    uint64_t now_ms = 0;
};
// "Maps" at the title: request() once the menu is ready; step() every game frame returns what to do next.
struct Machine {
    static constexpr unsigned CONTROL_FRAMES = 30;      // consecutive frames of control before the warp
    static constexpr unsigned GRACE_FRAMES = 3;         // after the menu writes, the tasks may still show the menu
    static constexpr uint64_t START_TIMEOUT_MS = 10000;  // the menu must be ready within this after the request
    static constexpr uint64_t TIMEOUT_MS = 240000;       // new game start to control, cutscenes included
    Phase phase = Phase::IDLE;
    uint32_t pending = 0;
    uint64_t started_ms = 0;
    unsigned control_frames = 0, since_start = 0;
    bool busy() const { return phase == Phase::NEWGAME || phase == Phase::WAIT_CONTROL; }
    bool request(uint32_t packed, bool title_ready, uint64_t now_ms) {
        if (busy() || !title_ready) return false;
        phase = Phase::NEWGAME; pending = packed; started_ms = now_ms; control_frames = since_start = 0; return true;
    }
    void reset() { phase = Phase::IDLE; pending = 0; control_frames = since_start = 0; }
    Step step(const Inputs& in) {
        switch (phase) {
        case Phase::IDLE: case Phase::DONE: return Step::NONE;
        case Phase::NEWGAME:
            if (in.title_ready) { phase = Phase::WAIT_CONTROL; since_start = 0; control_frames = 0; return Step::START_NEW_GAME; }
            if (in.now_ms - started_ms > START_TIMEOUT_MS) { reset(); return Step::ABORT_NOT_READY; }
            return Step::NONE;
        case Phase::WAIT_CONTROL:
            if (in.now_ms - started_ms > TIMEOUT_MS) { reset(); return Step::ABORT_TIMEOUT; }
            ++since_start;
            if (since_start > GRACE_FRAMES && menu_or_title(in.active)) { reset(); return Step::ABORT_MENU; }
            if (in.game_ready) { if (++control_frames >= CONTROL_FRAMES) { phase = Phase::DONE; return Step::WARP; } }
            else control_frames = 0;
            return Step::NONE;
        }
        return Step::NONE;
    }
};
}
