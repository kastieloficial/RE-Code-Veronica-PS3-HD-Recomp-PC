#pragma once
#include <cwchar>
#include <cstring>
#include "../../libs/system/cellSysutil.h"

namespace cv_original_language {
// Stable INI values; the dropdown order is Automatic, German, English.
inline constexpr const wchar_t* keys[] = {L"auto", L"de", L"en"};
inline int parse(const wchar_t* value) {
    for (int i = 0; i < 3; ++i) if (std::wcscmp(value, keys[i]) == 0) return i;
    return 0;
}
inline int system_language(int choice, const char* title, bool ptbr) {
    // Automatic deliberately preserves the original English system language.
    // PT-BR overlays are built against eng/usa archives, never ger archives.
    return !ptbr && std::strcmp(title, "NPEB00553") == 0 && choice == 1
        ? CELL_SYSUTIL_LANG_GERMAN : CELL_SYSUTIL_LANG_ENGLISH_US;
}
// Port-added label follows the original menu language; it is not PS3 game data.
inline const char* port_menu_title(const char* title,bool ptbr,const char* system) {
    if(ptbr)return "CONFIGURAÇÕES DO RECOMP";
    if(std::strcmp(title,"NPEB00553")==0 && system && std::strcmp(system,"4")==0)
        return "PORT-EINSTELLUNGEN";
    return "RECOMP SETTINGS";
}

}
