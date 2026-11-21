// The modal owner's decision logic (cv_port_modal.h Core): mutual exclusion, the 250 ms debounce,
// release of only its own bit, the pause rule and the single pad-block rule. No window, no game.
#include "../cv_port_modal.h"
#include <cassert>
#include <cstdio>
using namespace cv_port_modal;
int main() {
    {   // acquire / release one bit
        Core c;assert(!c.pad_block()&&!c.pause());
        assert(c.acquire(ITEMS,1000)&&c.mask==ITEMS&&c.pad_block()&&c.pause());
        c.release(ITEMS,1100);assert(c.mask==0&&!c.pad_block()&&!c.pause());
    }
    {   // exclusion: opening one non-launcher window names the others to close; the launcher coexists
        Core c;c.acquire(ARCHIVES,1000);
        assert(c.exclusive_conflict(ITEMS)==ARCHIVES);
        assert(c.exclusive_conflict(ARCHIVES)==0);
        c.set_launcher(true);assert(c.exclusive_conflict(ITEMS)==ARCHIVES);   // LAUNCHER is never a conflict
        Core d;d.set_launcher(true);assert(d.exclusive_conflict(TITLE_RETURN)==0);
        Core e;e.acquire(ITEMS,1000);assert(e.exclusive_conflict(TITLE_RETURN)==ITEMS&&e.exclusive_conflict(TEST_TOOLS)==ITEMS);
    }
    {   // debounce: a toggle within 250 ms of the last one is refused, later ones pass
        Core c;assert(c.acquire(ITEMS,1000));c.release(ITEMS,1100);
        assert(c.debounced(ITEMS,1100)&&c.debounced(ITEMS,1349));
        assert(!c.acquire(ITEMS,1200)&&c.mask==0);
        assert(!c.acquire(ITEMS,1349)&&c.mask==0);
        assert(!c.debounced(ITEMS,1350)&&c.acquire(ITEMS,1350)&&c.mask==ITEMS);
        assert(!c.debounced(ARCHIVES,1200));      // per bit
        assert(!c.debounced(5,1200)&&!c.debounced(LAUNCHER+ITEMS,0));   // not a single bit: never debounced
    }
    {   // acquiring what is already held is idempotent and does not restart the debounce
        Core c;c.acquire(ARCHIVES,1000);assert(c.acquire(ARCHIVES,1010)&&c.last_toggle[2]==1000);
    }
    {   // release clears only its own bit (and its working flag)
        Core c;c.acquire(ITEMS,1000);c.set_launcher(true);
        c.release(ARCHIVES,2000);assert(c.mask==(ITEMS|LAUNCHER));
        c.release(ITEMS,2000);assert(c.mask==LAUNCHER);
        c.set_launcher(false);assert(c.mask==0);
    }
    {   // pad block = any window; pause = any non-launcher window that is not working
        Core c;c.set_launcher(true);assert(c.pad_block()&&!c.pause());
        c.acquire(ARCHIVES,1000);assert(c.pad_block()&&c.pause());
        c.set_working(ARCHIVES,true);assert(c.pad_block()&&!c.pause());   // a save is being written: the game runs, the pad stays blocked
        c.set_working(ARCHIVES,false);assert(c.pause());
        c.set_working(ITEMS,true);assert(c.working==0);                   // only an open window can be working
        c.set_working(ARCHIVES,true);c.release(ARCHIVES,2000);assert(c.working==0&&c.mask==LAUNCHER&&!c.pause());
    }
    {   // unknown bits are ignored
        Core c;assert(!c.acquire(0,1000)&&!c.acquire(3,1000)&&c.mask==0);c.release(64);assert(c.mask==0);
    }
    assert(key_repeat(1<<30)&&!key_repeat(0)&&!key_repeat(0x00010001));
    std::puts("cv_port_modal core tests passed");
    return 0;
}
