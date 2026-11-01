/* cv_discord.cpp -- "Playing Resident Evil Code: Veronica X" on Discord.
 *
 * Discord's desktop app listens on a local named pipe (\\.\pipe\discord-ipc-0
 * to -9). A program shows its Rich Presence by sending a handshake with its
 * Application ID, then SET_ACTIVITY. No Discord SDK or DLL is needed: each
 * frame is a little-endian opcode, a length and a UTF-8 JSON body.
 *
 * Runs on its own thread from the moment the game boots. Without Discord
 * open it retries quietly every 20 s; it never blocks the game. The launcher's
 * "Show on Discord" option sets CV_DISCORD=0 to turn it off.
 *
 * The Application ID belongs to the "Resident Evil Code: Veronica X" app on
 * the Discord Developer Portal; its name is what Discord shows as the game.
 */
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>

#ifndef CV_DISCORD_APP_ID
#define CV_DISCORD_APP_ID "1566529230097875025"   /* Discord Developer Portal, the port's app */
#endif

extern "C" const wchar_t* cv_launcher_version(void);   /* launcher.cpp */

namespace {

enum : uint32_t { OP_HANDSHAKE = 0, OP_FRAME = 1, OP_CLOSE = 2 };

HANDLE open_pipe()
{
    for (int i = 0; i < 10; i++) {
        wchar_t name[64];
        swprintf(name, 64, L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    return INVALID_HANDLE_VALUE;
}

bool write_all(HANDLE h, const void* p, DWORD n)
{
    DWORD done = 0;
    return WriteFile(h, p, n, &done, nullptr) && done == n;
}

bool read_all(HANDLE h, void* p, DWORD n)
{
    char* c = (char*)p;
    while (n) {
        DWORD got = 0;
        if (!ReadFile(h, c, n, &got, nullptr) || !got) return false;
        c += got; n -= got;
    }
    return true;
}

/* One frame out, and Discord's answer read (and dropped). */
bool exchange(HANDLE h, uint32_t op, const std::string& json)
{
    const uint32_t head[2] = { op, (uint32_t)json.size() };
    if (!write_all(h, head, sizeof head) || !write_all(h, json.data(), (DWORD)json.size())) return false;
    uint32_t reply[2];
    if (!read_all(h, reply, sizeof reply) || reply[1] > (1u << 20)) return false;
    std::string body(reply[1], '\0');
    if (reply[1] && !read_all(h, &body[0], reply[1])) return false;
    return reply[0] != OP_CLOSE;
}

std::string narrow(const wchar_t* w)
{
    std::string s;
    for (; w && *w; ++w) s += (*w < 128 && *w != '"' && *w != '\\') ? (char)*w : '?';
    return s;
}

std::string activity_json(const char* title_id, long long started)
{
    const char* region = !strcmp(title_id, "NPUB30467") ? "US" : !strcmp(title_id, "NPJB00135") ? "JP" : "EU";
    const std::string version = narrow(cv_launcher_version());
    char buf[1024];
    snprintf(buf, sizeof buf,
        "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%lld\",\"args\":{\"pid\":%lu,\"activity\":{"
        "\"details\":\"PS3 recomp for PC (%s)\","
        "\"state\":\"Port %s\","
        "\"timestamps\":{\"start\":%lld},"
        "\"assets\":{\"large_image\":\"icon\",\"large_text\":\"Resident Evil Code: Veronica X\"},"
        "\"buttons\":[{\"label\":\"Get the PC port\",\"url\":\"https://github.com/kastieloficial/RE-Code-Veronica-PS3-HD-Recomp-PC\"}]"
        "}}}",
        (long long)GetTickCount64(), GetCurrentProcessId(), region, version.c_str(), started);
    return buf;
}

void run(std::string title_id)
{
    const long long started = (long long)time(nullptr);
    const std::string hello = std::string("{\"v\":1,\"client_id\":\"") + CV_DISCORD_APP_ID + "\"}";
    for (;;) {
        HANDLE h = open_pipe();
        if (h != INVALID_HANDLE_VALUE) {
            if (exchange(h, OP_HANDSHAKE, hello)) {
                fprintf(stderr, "[discord] presence on\n");
                /* Re-sent now and then: cheap, and it notices Discord closing. */
                while (exchange(h, OP_FRAME, activity_json(title_id.c_str(), started)))
                    Sleep(60000);
            }
            CloseHandle(h);
        }
        Sleep(20000);
    }
}

}  // namespace

/* Called once when the game boots. */
extern "C" void cv_discord_start(const char* title_id)
{
    const char* off = getenv("CV_DISCORD");
    if (off && *off == '0') return;
    if (!strcmp(CV_DISCORD_APP_ID, "0")) return;     /* no application configured */
    std::thread(run, std::string(title_id ? title_id : "")).detach();
}
