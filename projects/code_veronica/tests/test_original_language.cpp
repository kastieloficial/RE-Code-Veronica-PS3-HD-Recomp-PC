#include "cv_original_language.h"
#include "launcher_i18n.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

// Exercise the real HLE with a bounded big-endian guest-memory fixture.
static uint8_t memory[16];
extern "C" {
uint8_t* vm_base = memory;
// Unrelated HLE entry points must not be reached by a system-parameter query.
void cellMsgDialog_tick(void) { std::abort(); }
const char* cellGame_get_title_id(void) { std::abort(); }
void vm_write8(uint64_t address, uint8_t value) {
    assert(address < sizeof memory);
    memory[address] = value;
}
void vm_write32(uint64_t address, uint32_t value) {
    assert(address + 4 <= sizeof memory);
    for (unsigned i = 0; i < 4; ++i) memory[address + i] = uint8_t(value >> (24 - 8 * i));
}
}
static void language_env(const char* value) {
#ifdef _WIN32
    assert(_putenv_s("PS3_SYSTEM_LANGUAGE", value ? value : "") == 0);
#else
    assert((value ? setenv("PS3_SYSTEM_LANGUAGE", value, 1) : unsetenv("PS3_SYSTEM_LANGUAGE")) == 0);
#endif
}
static int parameter(int id) {
    assert(cellSysutilGetSystemParamInt(id, 4) == CELL_OK);
    return (uint32_t(memory[4]) << 24) | (uint32_t(memory[5]) << 16) |
           (uint32_t(memory[6]) << 8) | memory[7];
}
int main() {
    using namespace cv_original_language;
    assert(std::strcmp(port_menu_title("NPEB00553",false,"4"),"PORT-EINSTELLUNGEN")==0);
    assert(std::strcmp(port_menu_title("NPEB00553",true,"4"),"CONFIGURAÇÕES DO RECOMP")==0);
    assert(std::strcmp(port_menu_title("NPUB30467",false,"4"),"RECOMP SETTINGS")==0);
    assert(std::strcmp(port_menu_title("NPEB00553",false,nullptr),"RECOMP SETTINGS")==0);

    static_assert(CELL_SYSUTIL_LANG_GERMAN == 4);
    static_assert(CELL_SYSUTIL_LANG_ENGLISH_US == 1);
    const char* previous = std::getenv("PS3_SYSTEM_LANGUAGE");
    const bool had_previous = previous != nullptr;
    const std::string saved = previous ? previous : "";
    language_env(nullptr);
    assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG) == CELL_SYSUTIL_LANG_ENGLISH_US);
    assert(uint32_t(cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG, 0)) == CELL_SYSUTIL_ERROR_VALUE);
    for (int choice = 0; choice < 3; ++choice) {
        assert(parse(keys[choice]) == choice); // persistent INI value round trip
        for (const char* title : {"NPEB00553", "NPUB30467", "NPJB00135", "unknown"}) {
            for (bool ptbr : {false, true}) {
                const int expected = std::strcmp(title, "NPEB00553") == 0 && !ptbr && choice == 1 ? 4 : 1;
                const int selected = system_language(choice, title, ptbr);
                assert(selected == expected);
                language_env(std::to_string(selected).c_str());
                assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG) == expected);
                assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN) == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS);
                assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_PAD_RUMBLE) == 1);
                assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_TIMEZONE) == 0);
            }
        }
    }
    for (const wchar_t* invalid : {L"", L"invalid", L"4", L"DE", L"de-extra"}) assert(parse(invalid) == 0);
    for (int language = CELL_SYSUTIL_LANG_JAPANESE; language <= CELL_SYSUTIL_LANG_TURKISH; ++language) {
        language_env(std::to_string(language).c_str());
        assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG) == language);
    }
    for (const char* invalid : {"", "de", "4extra", "-1", "20", "999999999999999999999"}) {
        language_env(invalid);
        assert(parameter(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG) == CELL_SYSUTIL_LANG_ENGLISH_US);
    }
    for (int lang = 0; lang < LANG_COUNT; ++lang)
        for (int column = 0; column < 4; ++column) assert(k_original_language_text[lang][column][0]);
    language_env(had_previous ? saved.c_str() : nullptr);
    std::puts("Original language: EU/US/JP, PT-BR, persistence values, HLE guest writes and defaults OK");
}
