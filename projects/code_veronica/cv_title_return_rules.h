#pragma once
/* "Return to the title screen" (F2 / L1+L2+R1+R2+Start+Select): when the box may open, as pure rules on a snapshot of the
 * system block, and the pure parts of the game's own reset it triggers. No guest memory, no Win32: tests/test_title_return.cpp
 * checks the matrix, cv_title_return.cpp reads the guest (UI thread to open the box, game thread to act on Yes) and asks here.
 *
 * Task bits are the ones of system+0x64: tasks = +0x64, blocked = +0x68, active = tasks & ~blocked (docs/PLANO-IMPLEMENTACAO-v1.7.md).
 *   0x4 title sequence, 0x8 main menu, 0x80 the room (gameplay), 0x200 the game's inventory screen, 0x8000 the save / load screen.
 *   +0x70 0x08000000 the typewriter, 0x1C040008 event / movie flags (0x8 is also set while the inventory screen pauses the room);
 *   +0x74 & 7 a door or room change, 0x00200000 a save screen request, 0x2 a fade. */
#include <cstdint>

namespace cv_title_return {
constexpr uint32_t TASK_TITLE = 0x4, TASK_MENU = 0x8, TASK_ROOM = 0x80, TASK_INVENTORY = 0x200, TASK_SAVE = 0x8000;
constexpr uint32_t TASKS_NOT_PLAIN = 0x1803F;           // title, menus, setup, the save screen: never under the box (cv_items.cpp's mask)
constexpr uint32_t F70_TYPEWRITER = 0x08000000, F70_INVENTORY = 0x8, F70_EVENT = 0x1C040008;
constexpr uint32_t F74_TRANSITION = 0x7, F74_SAVE_REQUEST = 0x00200000;
constexpr uint32_t TASKS_AFTER_RETURN = 0x300048;       // system+0x64 once the game is back at the main menu
constexpr uint32_t TASKS_AFTER_RETURN_TITLE = 0x300044; // ... or at the title sequence (the game picks one, by +0x50 & 0x200000)

// The game's own way back to the title, the one the REINICIAR JOGO row of the Select options uses (docs/PLANO-IMPLEMENTACAO-v1.7.md,
// "Return to title, as built"): the options menu closes after a "Yes" and raises the request word system-0x3B0 (EU 0xBC3458, BC3458 in
// the notes). The game's own reset task 0x200000 (EU func_001B2A48), which runs every frame beside the room, then does the whole return
// (func_001B0054: tasks = 0x300048 or 0x300044, +0x68 = 0, menu state reset) when
//   +0x50 has none of 0x120000 (0x20000: the main menu was left by a shortcut, 0x100000: a reset already done), +0x50 has no 0x400000
//   (a game started by the menu's "Continue" branch: that one ends the program's systems instead), +0x64 & 0x3F == 0.
constexpr uint32_t TASK_RESET_WATCHER = 0x00200000, TASKS_LOW = 0x3F;
constexpr uint32_t F50_RESET_BLOCKERS = 0x00120000, F50_MENU_CONTINUE = 0x00400000;
constexpr bool watcher_running(uint32_t tasks, uint32_t blocked) { return ((tasks & ~blocked) & TASK_RESET_WATCHER) != 0; }
// Raising the request word is only safe when the reset task will see it in the same frame (a second watcher, after the task loop,
// would run the program's shutdown on it): the reset task must be running and no early task bit may be set.
constexpr bool can_arm(uint32_t tasks, uint32_t blocked) { return watcher_running(tasks, blocked) && (tasks & TASKS_LOW) == 0; }
constexpr uint32_t armed_flags50(uint32_t flags50) { return flags50 & ~(F50_RESET_BLOCKERS | F50_MENU_CONTINUE); }

// The game's own inventory screen is allowed (the box can come from it). Kept as a switch: if the first run at the user's
// shows that the screen leaves something on top of the title, set it false and the box refuses there (docs plan, P4 risk).
constexpr bool ALLOW_INVENTORY_SCREEN = true;

struct State {
    bool mapped = false;        // the system block and the SRT words are readable
    bool live_character = false;// a character whose room data is live (cv_test_tools.cpp live_character)
    int32_t health = 0;         // 32-bit, as the SRT reads it
    uint32_t tasks = 0, blocked = 0, flags70 = 0, flags74 = 0;
};

enum class Verdict {
    GAME,               // plain gameplay: allowed
    INVENTORY_SCREEN,   // the game's inventory screen: allowed
    UNREADABLE,         // the guest is not there yet
    MENU_OR_TITLE,      // the title sequence or the main menu
    SAVE_LOAD,          // saving, loading, the typewriter
    GAME_OVER,          // no live character or no health
    TRANSITION,         // a door or a room change
    EVENT,              // a cutscene, a movie or another scripted moment
    OTHER_SCREEN,       // another task is running (a screen the game does not name here)
    INVENTORY_REFUSED   // the inventory screen with ALLOW_INVENTORY_SCREEN off
};
constexpr bool allowed(Verdict v) { return v == Verdict::GAME || v == Verdict::INVENTORY_SCREEN; }

constexpr uint32_t active_tasks(const State& s) { return s.tasks & ~s.blocked; }
// The inventory screen as the game's own run logged it: the inventory task without the room task, and the pause flag in +0x70.
constexpr bool inventory_screen(const State& s) {
    return (active_tasks(s) & TASK_INVENTORY) && !(active_tasks(s) & TASK_ROOM) && (s.flags70 & F70_INVENTORY);
}

constexpr Verdict evaluate(const State& s) {
    if (!s.mapped) return Verdict::UNREADABLE;
    const uint32_t active = active_tasks(s);
    if ((active & (TASK_TITLE | TASK_MENU)) && !(active & TASK_ROOM)) return Verdict::MENU_OR_TITLE;
    if ((active & TASK_SAVE) || (s.flags74 & F74_SAVE_REQUEST) || (s.flags70 & F70_TYPEWRITER)) return Verdict::SAVE_LOAD;
    if (!s.live_character || s.health <= 0) return Verdict::GAME_OVER;
    if (s.flags74 & F74_TRANSITION) return Verdict::TRANSITION;
    const bool inventory = inventory_screen(s);
    // The inventory screen sets F70 0x8 itself; every other event flag still refuses it.
    if (s.flags70 & (inventory ? (F70_EVENT & ~F70_INVENTORY) : F70_EVENT)) return Verdict::EVENT;
    if (active & TASKS_NOT_PLAIN) return Verdict::OTHER_SCREEN;
    if (inventory) return ALLOW_INVENTORY_SCREEN ? Verdict::INVENTORY_SCREEN : Verdict::INVENTORY_REFUSED;
    if (active & TASK_ROOM) return Verdict::GAME;
    return Verdict::OTHER_SCREEN;
}
constexpr bool can_open(const State& s) { return allowed(evaluate(s)); }

// The game thread raises the request word and waits: the game's reset task acts on it and the tasks read one of these two words.
constexpr bool returned(uint32_t tasks) { return tasks == TASKS_AFTER_RETURN || tasks == TASKS_AFTER_RETURN_TITLE; }
constexpr unsigned MAX_FRAMES = 600;                    // then give up and say so in the log

enum class Step { IDLE, START, CALL, DONE, ABORT };
struct Machine {
    bool running = false;
    unsigned frames = 0;
    // `requested`: the window's Yes; `allowed_now`: the rules on the game thread; `tasks`: system+0x64 after the last call.
    Step step(bool requested, bool allowed_now, uint32_t tasks) {
        if (!running) {
            if (!requested) return Step::IDLE;
            if (!allowed_now) return Step::ABORT;
            running = true; frames = 0; return Step::START;
        }
        if (returned(tasks)) { running = false; return Step::DONE; }
        if (++frames >= MAX_FRAMES) { running = false; return Step::ABORT; }
        return Step::CALL;
    }
    void finish() { running = false; }   // the game has reached the title (or the request was dropped)
};
}
