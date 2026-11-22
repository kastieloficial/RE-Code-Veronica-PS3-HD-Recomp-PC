// Return to the title, the pure parts: when the box may open (the allowed / blocked matrix on snapshots of the system block),
// the game-thread machine (start, frames, the ending, the give-up), the controller chord (rising edge only with the shoulders
// held first) and the shortcut gate's release tail (Start / Select stay hidden from the game while the chord is being built).
#include "../cv_title_return_rules.h"
#include "../cv_launcher_controls.h"
#include "../../../libs/input/pad_shortcut_gate.h"
#include <cassert>
#include <cstdio>
using namespace cv_title_return;
namespace lc=cv_launcher_controls;

// Plain gameplay: a live character with health, the room task running, nothing else in the words.
static State gameplay() {State s;s.mapped=true;s.live_character=true;s.health=150;s.tasks=0x80;return s;}
// The game's inventory screen, as a JP run logged it (cv_items tests use the same words).
static State inventory() {State s=gameplay();s.tasks=0x0073DFC0;s.blocked=0x0003DC80;s.flags70=0x00015408;s.flags74=0x40;return s;}

int main() {
    {   // allowed
        assert(evaluate(gameplay())==Verdict::GAME&&can_open(gameplay()));
        State s=gameplay();s.flags70=0x00015400;s.flags74=0x40;assert(evaluate(s)==Verdict::GAME);   // flags the room runs with
        assert(active_tasks(inventory())==0x00700340u&&!(active_tasks(inventory())&TASK_ROOM));
        assert(inventory_screen(inventory())&&!inventory_screen(gameplay()));
        assert(evaluate(inventory())==Verdict::INVENTORY_SCREEN&&can_open(inventory()));
        State b=gameplay();b.blocked=0x100;assert(evaluate(b)==Verdict::GAME);   // a blocked task bit does not count as running
    }
    {   // title and menu
        State s=gameplay();s.tasks=0x4;s.live_character=false;assert(evaluate(s)==Verdict::MENU_OR_TITLE);
        s=gameplay();s.tasks=0x8;assert(evaluate(s)==Verdict::MENU_OR_TITLE);   // stale character data at the menu
        s=gameplay();s.tasks=0xC;assert(evaluate(s)==Verdict::MENU_OR_TITLE);
        s=gameplay();s.tasks=0x8;s.blocked=0x8;assert(evaluate(s)==Verdict::OTHER_SCREEN);   // blocked: not running; nothing else is either
    }
    {   // saving and loading
        State s=gameplay();s.tasks=0x80|0x8000;assert(evaluate(s)==Verdict::SAVE_LOAD);          // the save / load screen
        s=gameplay();s.flags74=0x00200000;assert(evaluate(s)==Verdict::SAVE_LOAD);               // a save screen being asked for
        s=gameplay();s.flags70=0x08000000;assert(evaluate(s)==Verdict::SAVE_LOAD);               // the typewriter
        s=inventory();s.tasks|=0x8000;s.blocked&=~0x8000u;assert(evaluate(s)==Verdict::SAVE_LOAD);
    }
    {   // Game Over / no character
        State s=gameplay();s.health=0;assert(evaluate(s)==Verdict::GAME_OVER);
        s=gameplay();s.health=-5;assert(evaluate(s)==Verdict::GAME_OVER);
        s=gameplay();s.live_character=false;assert(evaluate(s)==Verdict::GAME_OVER);
        s=gameplay();s.mapped=false;assert(evaluate(s)==Verdict::UNREADABLE&&!can_open(s));
        s=inventory();s.health=0;assert(evaluate(s)==Verdict::GAME_OVER);
    }
    {   // a door or room change
        for(uint32_t f:{1u,2u,4u,7u}){State s=gameplay();s.flags74=f;assert(evaluate(s)==Verdict::TRANSITION);}
        State s=inventory();s.flags74=0x41;assert(evaluate(s)==Verdict::TRANSITION);
        s=gameplay();s.flags74=0x2;assert(evaluate(s)==Verdict::TRANSITION);   // a fade is a bit of the same mask
    }
    {   // events and movies: every bit of the mask refuses; the inventory screen's own 0x8 does not
        for(uint32_t f:{0x8u,0x40000u,0x04000000u,0x10000000u}){State s=gameplay();s.flags70=f;assert((f&F70_EVENT)&&evaluate(s)==Verdict::EVENT);}   // (0x08000000 is the typewriter: SAVE_LOAD)
        State s=gameplay();s.flags70=0x8;assert(evaluate(s)==Verdict::EVENT);                       // 0x8 outside the inventory screen: an event
        s=gameplay();s.flags70=0x00040000;assert(evaluate(s)==Verdict::EVENT);
        s=gameplay();s.flags70=0x04000000;assert(evaluate(s)==Verdict::EVENT);
        s=inventory();s.flags70|=0x04000000;assert(evaluate(s)==Verdict::EVENT);                    // an event flag still refuses the inventory screen
        s=inventory();s.flags70|=0x00040000;assert(evaluate(s)==Verdict::EVENT);
    }
    {   // other screens: the setup task, a menu task of the room, the 0x10000 task
        for(uint32_t t:{0x80u|0x1u,0x80u|0x2u,0x80u|0x10u,0x80u|0x20u,0x80u|0x10000u}){State s=gameplay();s.tasks=t;assert(evaluate(s)==Verdict::OTHER_SCREEN);}
        State s=gameplay();s.tasks=0;assert(evaluate(s)==Verdict::OTHER_SCREEN);
        s=gameplay();s.tasks=0x200;s.flags70=0;assert(evaluate(s)==Verdict::OTHER_SCREEN);          // the inventory task without its pause flag
        s=gameplay();s.tasks=0x80|0x200;s.flags70=0x8;assert(evaluate(s)==Verdict::EVENT);          // both room and inventory tasks: not the pause state
    }
    {   // the ending and the machine
        Machine m;
        assert(m.step(false,false,0)==Step::IDLE&&!m.running);
        assert(m.step(true,false,0)==Step::ABORT&&!m.running);            // refused on the game thread
        assert(m.step(true,true,0x80)==Step::START&&m.running);
        for(unsigned i=1;i<MAX_FRAMES;i++)assert(m.step(false,false,0x80)==Step::CALL);
        assert(m.step(false,false,0x80)==Step::ABORT&&!m.running);       // 600 frames: give up
        assert(m.step(true,true,0x80)==Step::START);m.frames=10;
        assert(m.step(false,false,TASKS_AFTER_RETURN)==Step::DONE&&!m.running);
        assert(m.step(true,true,0x80)==Step::START);m.finish();assert(!m.running&&m.step(false,false,0)==Step::IDLE);
        assert(returned(0x300048)&&!returned(0x300050)&&!returned(0x80));
        assert(TASKS_AFTER_RETURN==0x00300048);
    }
    {   // the chord: Start+Select with the four shoulders already held
        const uint32_t sh=lc::shoulders,ss=lc::start_select;
        assert(lc::title_chord==(sh|ss));
        assert(lc::title_chord_rising(sh,sh|ss));                                  // both pressed in one sample
        assert(lc::title_chord_rising(sh|(1u<<PB_START),sh|ss));                    // Start first, then Select
        assert(lc::title_chord_rising(sh|(1u<<PB_SELECT),sh|ss));
        assert(!lc::title_chord_rising(sh|ss,sh|ss));                              // held: not a new edge
        assert(!lc::title_chord_rising(ss,sh|ss));                                 // shoulders pressed last: nothing
        assert(!lc::title_chord_rising(sh&~(1u<<PB_R2),sh|ss));                     // one shoulder missing before
        assert(!lc::title_chord_rising(sh,sh|(1u<<PB_START)));                      // Start alone
        assert(!lc::title_chord_rising(sh,ss));                                    // shoulders released as Start+Select came
        assert(lc::title_chord_rising(sh|(1u<<PB_SOUTH),sh|ss|(1u<<PB_SOUTH)));    // other buttons do not matter
        assert(!lc::title_chord_rising(0,0));
    }
    {   // the shoulder-prefix gate: Start / Select never reach the game while the chord is built, nor after the shoulders go
        const uint32_t sh=lc::shoulders;unsigned latch=0,other=0;
        assert(!pad_shortcut_gate(&latch,1u<<PB_START));                  // Start alone belongs to the game (pause)
        assert(pad_shortcut_gate(&latch,sh));                              // shoulders: hidden
        assert(pad_shortcut_gate(&latch,sh|(1u<<PB_START)));
        assert(pad_shortcut_gate(&latch,sh|lc::start_select));
        assert(pad_shortcut_gate(&latch,lc::start_select));                // shoulders let go first: Start+Select stay hidden
        assert(pad_shortcut_gate(&latch,1u<<PB_START));                    // the tail: only Start still held
        assert(pad_shortcut_gate(&latch,1u<<PB_SELECT));
        assert(pad_shortcut_gate(&latch,0));                               // all released: the last hidden frame
        assert(!pad_shortcut_gate(&latch,1u<<PB_START));                   // and Start is the game's again
        assert(!pad_shortcut_gate(&other,lc::start_select));               // another controller is not latched
        // Start pressed before the shoulders: the game saw it once, nothing is hidden retroactively; the latch starts with the shoulders
        unsigned late=0;assert(!pad_shortcut_gate(&late,1u<<PB_SELECT));assert(pad_shortcut_gate(&late,sh|(1u<<PB_SELECT)));assert(pad_shortcut_gate(&late,1u<<PB_SELECT));
    }
    std::puts("title return rules, machine, chord and gate: ok");
    return 0;
}
