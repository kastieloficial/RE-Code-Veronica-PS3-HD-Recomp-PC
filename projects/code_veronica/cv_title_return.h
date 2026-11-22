#pragma once
/* Return to the title screen: F2 in the game, or L1+L2+R1+R2+Start+Select on a controller.
 * The box (cv_title_return_ui.h) lives on the launcher thread; "Yes" files a request that the game thread
 * (the top of the game's task dispatcher, cv_test_tools.cpp TEST_FRAME) carries out by running the Game Over
 * return task until the game is back at its main menu. Rules: cv_title_return_rules.h. */
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
struct ppu_context;
void cv_title_return_toggle(int language,int opening_input=0);   // UI thread: opens the box (or a notice when it cannot), or closes it
void cv_title_return_close();                                    // the box as "No"
bool cv_title_return_visible();
void cv_title_return_frame(ppu_context* ctx);                    // game thread, once a game frame: the request after "Yes"
void cv_title_return_release();                                  // before GdiplusShutdown / at exit
#endif
