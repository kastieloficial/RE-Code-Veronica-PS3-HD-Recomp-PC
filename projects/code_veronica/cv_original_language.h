#pragma once
#include <cwchar>
#include <cstring>
#include "../../libs/system/cellSysutil.h"

namespace cv_original_language {
// Preserve existing auto/de/en INI values and indices; append native EU languages.
inline constexpr const wchar_t* keys[] = {L"auto", L"de", L"en", L"fr", L"it", L"es"};
inline constexpr int count=sizeof(keys)/sizeof(keys[0]);
inline int parse(const wchar_t* value) {
    if(!value)return 0;
    for (int i = 0; i < count; ++i) if (std::wcscmp(value, keys[i]) == 0) return i;
    return 0;
}
inline int system_language(int choice, const char* title, bool ptbr) {
    // PT-BR overlays target eng/usa. Never apply another native language
    // under that translation. US additional locales use a read-only EU resource overlay.
    static constexpr int language[]={CELL_SYSUTIL_LANG_ENGLISH_US,CELL_SYSUTIL_LANG_GERMAN,
        CELL_SYSUTIL_LANG_ENGLISH_US,CELL_SYSUTIL_LANG_FRENCH,CELL_SYSUTIL_LANG_ITALIAN,CELL_SYSUTIL_LANG_SPANISH};
    return !ptbr && title && (std::strcmp(title,"NPEB00553")==0||std::strcmp(title,"NPUB30467")==0) && choice>=0 && choice<count
        ? language[choice] : CELL_SYSUTIL_LANG_ENGLISH_US;
}
// Unified game-text selector: Original, PT-BR, English, French, German,
// Italian, Spanish. The launcher exposes these for EU/US; JP is disabled.
inline constexpr int text_original_choices[]={0,0,2,3,1,4,5};
inline int text_entry(int choice,bool ptbr,bool european){
    if(ptbr)return 1;
    if(european)for(int i=2;i<7;i++)if(text_original_choices[i]==choice)return i;
    return 0;
}
inline bool apply_text_entry(int entry,bool european,int& choice,bool& ptbr){
    if(entry<0||entry>=(european?7:2))return false;
    ptbr=entry==1;
    if(!ptbr)choice=text_original_choices[entry];
    return true;
}
// Port-added label follows the original menu language; it is not PS3 game data.
inline const char* port_menu_title(const char* title,bool ptbr,const char* system) {
    if(ptbr)return "CONFIGURAÇÕES DO RECOMP";
    if((std::strcmp(title,"NPEB00553")==0||std::strcmp(title,"NPUB30467")==0) && system && std::strcmp(system,"4")==0)
        return "PORT-EINSTELLUNGEN";
    return "RECOMP SETTINGS";
}

}
