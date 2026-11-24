/* cv_fast_door.cpp -- Fast Door Skip + Sync I/O + Native x64 zlib Inflate for RE:CVX (PS3 EU).
 *
 * Bottlenecks fixed:
 *  1. bhSysCallDoordemo (0x001AFAE8): jump straight from case 0 to case 10
 *     (no-animation load screen path), skipping bhInitDoor / bhReadDoorData /
 *     bhSetDoor / bhExitDoor, and execute case 11 in the same frame.
 *  2. bhSysCallMonitor (0x001B0CB0) step 10 (func_001E2314):
 *     Stop Sound Bank 9 (CV_FN_STOP_BANK) so step 10 doesn't stall for ~3000 ms
 *     waiting for door/room SE to finish playing.
 *  3. bhSysCallMonitor + bhSysCallSndMonitor (0x001B2A48):
 *     Collapse synchronous step-per-frame transitions into a single frame.
 *  4. MtFile::read (0x007846D0):
 *     Avoid spawning & joining ~32 PPU threads ("readAsyncProc") per room .arc
 *     by syncing the OS fd via MtFile::seek and reading synchronously.
 *  5. decodeProc SPU zlib inflate (0x00538AEC / 0x00539B0C / loc_00539EB0):
 *     Replace per-chunk SPU zlib inflate jobs (0x0099F880 / 0x00917E00) with
 *     native x64 zlib 1.3.2 inflate() directly on guest memory.
 *
 * Environment controls:
 *   CV_FAST_DOOR=0       -- disable fast door hooks
 *   CV_FAST_DOOR_FADE=1  -- snap door fade-out / fade-in to 1 frame
 *   CV_FAST_DOOR_SYNC=0  -- disable synchronous MtFile::read optimization
 *   CV_NATIVE_ZLIB=0     -- disable native x64 zlib inflate (fall back to SPU)
 *   CV_FAST_DOOR_LOG=1   -- log detailed per-stage timing to stderr and door-profile.txt
 */
#include "ppu_recomp.h"
#include "cv_version.h"
#include "zlib.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#if !defined(CV_REGION_JP) && !defined(CV_REGION_US)

void cv_original_001AFAE8(ppu_context* ctx); /* bhSysCallDoordemo */
void cv_original_001B0CB0(ppu_context* ctx); /* bhSysCallMonitor */
void cv_original_001B2A48(ppu_context* ctx); /* bhSysCallSndMonitor */
void cv_original_007846D0(ppu_context* ctx); /* MtFile::read (sync/async) */
void cv_original_0078575C(ppu_context* ctx); /* MtFile::readAsync */
void cv_original_00538AEC(ppu_context* ctx); /* decodeProc zlib stream ctor */
void cv_original_00539B0C(ppu_context* ctx); /* decodeProc zlib stream dtor */
void cv_original_001CF5D4(ppu_context* ctx); /* njCalcPoint4 (mat4 * vec3) */
void cv_original_00287750(ppu_context* ctx); /* K-way draw-call merge sort */
void cv_original_001D32F0(ppu_context* ctx); /* vertex + normal + bbox transform */
void cv_original_001CE930(ppu_context* ctx); /* njMultiMatrix (affine 3x4) */
void cv_original_004BAAC0(ppu_context* ctx); /* vertex attribute packer (14 formats) */
void cv_original_0001D5D0(ppu_context* ctx); /* vertex buffer builder (strips/lists) */
void cv_original_00286E74(ppu_context* ctx); /* 2-way bottom-up merge sort (8B pairs) */
void cv_original_0027EF90(ppu_context* ctx); /* vertex buffer + draw desc allocator */
void cv_original_000124F4(ppu_context* ctx); /* atomic scratch bump allocator */
void cv_original_00549F7C(ppu_context* ctx); /* recursive tree traversal */
void cv_original_001D2028(ppu_context* ctx); /* njCnkDrawModel */
void cv_ui_original_enqueue(ppu_context* ctx); /* UI/draw command vector push_back */
void func_007858F4(ppu_context* ctx);        /* MtFile::seek(obj, off, origin) */
void CV_FN_STOP_BANK(ppu_context* ctx);      /* (sound_mgr, bank_id) */

void func_001B2A48(ppu_context* ctx);

extern "C" uint8_t* vm_base;
extern "C" void ps3_indirect_call(ppu_context* ctx);
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" volatile unsigned char* ps3_patch_flag(const char* name);

static bool frustum_log_enabled();
static void cv_frustum_record_32f0(uint32_t count, bool all_offscreen);

namespace {

constexpr uint32_t kSysPtrAddr   = 0x00A89BD0u; /* EU: (0xA9 << 16) - 0x6430 */
constexpr uint32_t kFileStatAddr = 0x00AD3E6Cu; /* EU: GetReadFileStatus() */
constexpr uint32_t kOffSsFlg     = 0x00050u;    /* bit 0x400000 = attract demo */
constexpr uint32_t kOffTsFlg     = 0x00068u;    /* bit 0x800 = door ready */
constexpr uint32_t kOffCbFlg     = 0x00074u;    /* bit 0x2 = screen fade active */
constexpr uint32_t kOffMnMd0     = 0x1B108u;    /* 4 = room load */
constexpr uint32_t kOffMnMd1     = 0x1B109u;    /* sub-step inside room load */
constexpr uint32_t kOffSdmFlg    = 0x1B130u;    /* bit 0x1 = sound monitor active */
constexpr uint32_t kOffSdmMode   = 0x1B134u;    /* sound monitor state machine */
constexpr uint32_t kOffFadeCt    = 0x2A5CCu;    /* float fade_ct */
constexpr uint32_t kOffFadeHkn   = 0x2A5D0u;    /* float fade_hkn (1.0f = finish in 1 frame) */
constexpr uint32_t kOffFadeAn    = 0x2A5D8u;    /* float fade_an */
constexpr uint32_t kOffFadeAp    = 0x2A5DCu;    /* float fade_ap */
constexpr uint32_t kOffFadeWait  = 0x2A608u;    /* uint32 fade delay frames */
constexpr uint32_t kOffDoorMode0 = 0x2A6ACu;    /* sys->door.mode0 (door + 0x8) */

/* Safe nested guest call (same pattern as cv_title_return.cpp). */
struct GuestCall {
    ppu_context c;
    ppu_context* previous;
    void (*tail)(void*);

    explicit GuestCall(ppu_context* p)
        : c(*p), previous(g_active_ctx), tail(g_trampoline_fn)
    {
        c.gpr[1] -= 0x2000;
        vm_write64(c.gpr[1], p->gpr[1]);
        c.gpr[2] = CV_TOC;
        g_active_ctx = &c;
        g_trampoline_fn = nullptr;
    }

    ~GuestCall()
    {
        g_active_ctx = previous;
        g_trampoline_fn = tail;
    }

    void run(void (*fn)(ppu_context*), uint32_t a, uint32_t b, uint32_t d = 0)
    {
        c.gpr[3] = a;
        c.gpr[4] = b;
        c.gpr[5] = d;
        fn(&c);
        while (g_trampoline_fn) {
            auto next = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            next(&c);
        }
    }

    uint32_t run_u32(void (*fn)(ppu_context*), uint32_t a, uint32_t b)
    {
        run(fn, a, b, 0);
        return (uint32_t)c.gpr[3];
    }

    uint32_t run_indirect_u32(uint32_t fn_addr, uint32_t a, uint32_t b, uint32_t d = 0)
    {
        c.gpr[3] = a;
        c.gpr[4] = b;
        c.gpr[5] = d;
        c.ctr = fn_addr;
        ps3_indirect_call(&c);
        while (g_trampoline_fn) {
            auto next = g_trampoline_fn;
            g_trampoline_fn = nullptr;
            next(&c);
        }
        return (uint32_t)c.gpr[3];
    }
};

void stop_sound_bank(ppu_context* ctx, uint32_t bank)
{
    const uint32_t sound = vm_read32(CV_SOUND_MANAGER);
    if (!sound) return;
    GuestCall call(ctx);
    call.run(CV_FN_STOP_BANK, sound, bank);
}

bool env_flag(const char* name, bool default_val)
{
    const char* v = std::getenv(name);
    if (!v || !*v) return default_val;
    return v[0] != '0';
}

bool fast_door_enabled()
{
    static int cached_env = -1;
    if (cached_env < 0) cached_env = env_flag("CV_FAST_DOOR", true) ? 1 : 0;
    if (!cached_env) return false;

    static volatile unsigned char* door_flag = nullptr;
    if (!door_flag) door_flag = ps3_patch_flag("cv_door_skip");
    return door_flag && (*door_flag != 0);
}

bool fast_fade_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_FAST_DOOR_FADE", false) ? 1 : 0;
    return cached != 0;
}

bool fast_sync_read_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_FAST_DOOR_SYNC", true) ? 1 : 0;
    return cached != 0;
}

bool native_zlib_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_NATIVE_ZLIB", true) ? 1 : 0;
    return cached != 0;
}

bool native_hot_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_NATIVE_HOT", true) ? 1 : 0;
    return cached != 0;
}

static inline uint32_t bswap32_fast(uint32_t v)
{
#ifdef _MSC_VER
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}

static inline uint16_t read_be16_fast(const uint8_t* p)
{
    uint16_t v;
    std::memcpy(&v, p, 2);
#ifdef _MSC_VER
    return _byteswap_ushort(v);
#else
    return __builtin_bswap16(v);
#endif
}

static inline uint32_t read_be32_fast(const uint8_t* p)
{
    uint32_t v;
    std::memcpy(&v, p, 4);
    return bswap32_fast(v);
}

static inline float read_bef32_fast(const uint8_t* p)
{
    uint32_t u = read_be32_fast(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

static inline void write_be32_fast(uint8_t* p, uint32_t v)
{
#ifdef _MSC_VER
    v = _byteswap_ulong(v);
#else
    v = __builtin_bswap32(v);
#endif
    std::memcpy(p, &v, 4);
}

static inline void write_be16_fast(uint8_t* p, uint16_t v)
{
#ifdef _MSC_VER
    v = _byteswap_ushort(v);
#else
    v = __builtin_bswap16(v);
#endif
    std::memcpy(p, &v, 2);
}

static inline void write_bef32_fast(uint8_t* p, float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
#ifdef _MSC_VER
    u = _byteswap_ulong(u);
#else
    u = __builtin_bswap32(u);
#endif
    std::memcpy(p, &u, 4);
}

bool fast_log_enabled()
{
    static int cached = -1;
    if (cached < 0) cached = env_flag("CV_FAST_DOOR_LOG", true) ? 1 : 0;
    return cached != 0;
}

uint32_t get_sys()
{
    return vm_read32(kSysPtrAddr);
}

bool is_attract_demo(uint32_t sys)
{
    return (vm_read32(sys + kOffSsFlg) & 0x00400000u) != 0;
}

void write_float(uint32_t addr, float val)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &val, sizeof(bits));
    vm_write32(addr, bits);
}

void snap_active_fade(uint32_t sys)
{
    const uint32_t cb = vm_read32(sys + kOffCbFlg);
    if (!(cb & 0x2u)) return;
    vm_write32(sys + kOffFadeWait, 0u);
    write_float(sys + kOffFadeHkn, 1.0f);
    write_float(sys + kOffFadeCt, 1.0f);
    vm_write32(sys + kOffFadeAn, vm_read32(sys + kOffFadeAp));
    vm_write32(sys + kOffCbFlg, cb & ~0x2u);
}

void log_line(const char* line)
{
    if (!fast_log_enabled()) return;
    static bool header_written = false;
    std::fprintf(stderr, "%s\n", line);
    std::fflush(stderr);
    if (FILE* f = std::fopen("D:\\ps3recomp-fast-door\\door-profile.txt", "a")) {
        if (!header_written) {
            std::fprintf(f, "--- NEW SESSION (v5b: stop bank 9 + sync read + native x64 zlib fixed) ---\n");
            header_written = true;
        }
        std::fprintf(f, "%s\n", line);
        std::fclose(f);
    }
}

std::atomic<int> g_door_read_calls{0};
std::atomic<long long> g_door_read_us{0};
std::atomic<int> g_door_inflate_calls{0};
std::atomic<long long> g_door_inflate_us{0};

/* Native x64 zlib stream pool keyed by guest stream object address. */
struct NativeZlibSlot {
    uint32_t addr = 0;
    bool active = false;
    z_stream zs{};
};

constexpr int kMaxZlibSlots = 64;
std::mutex g_zlib_mu;
NativeZlibSlot g_zlib_slots[kMaxZlibSlots];

NativeZlibSlot* zlib_slot_create(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    NativeZlibSlot* free_slot = nullptr;
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) {
            inflateEnd(&s.zs);
            std::memset(&s.zs, 0, sizeof(s.zs));
            if (inflateInit(&s.zs) == Z_OK) return &s;
            s.active = false;
            return nullptr;
        }
        if (!s.active && !free_slot) free_slot = &s;
    }
    if (!free_slot) return nullptr;
    std::memset(&free_slot->zs, 0, sizeof(free_slot->zs));
    if (inflateInit(&free_slot->zs) != Z_OK) return nullptr;
    free_slot->addr = addr;
    free_slot->active = true;
    return free_slot;
}

NativeZlibSlot* zlib_slot_find(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) return &s;
    }
    return nullptr;
}

void zlib_slot_destroy(uint32_t addr)
{
    std::lock_guard<std::mutex> lk(g_zlib_mu);
    for (auto& s : g_zlib_slots) {
        if (s.active && s.addr == addr) {
            inflateEnd(&s.zs);
            std::memset(&s.zs, 0, sizeof(s.zs));
            s.addr = 0;
            s.active = false;
            return;
        }
    }
}

struct DoorProfiler {
    using clock = std::chrono::steady_clock;
    bool active = false;
    bool fade_in_pending = false;
    bool stopped_bank9 = false;
    bool stopped_bank10 = false;
    int door_frames = 0;
    int monitor_frames = 0;
    int step_frames[24]{};
    int wait_file_frames = 0;
    int wait_snd_frames = 0;
    int wait_door_flag_frames = 0;
    int wait_fade_frames = 0;
    clock::time_point start_time{};
    clock::time_point last_change{};
    uint8_t cur_md1 = 255;
    long long step_ms[24]{};

    void begin()
    {
        active = true;
        stopped_bank9 = false;
        stopped_bank10 = false;
        door_frames = 0;
        monitor_frames = 0;
        wait_file_frames = 0;
        wait_snd_frames = 0;
        wait_door_flag_frames = 0;
        wait_fade_frames = 0;
        g_door_read_calls.store(0, std::memory_order_relaxed);
        g_door_read_us.store(0, std::memory_order_relaxed);
        g_door_inflate_calls.store(0, std::memory_order_relaxed);
        g_door_inflate_us.store(0, std::memory_order_relaxed);
        std::memset(step_frames, 0, sizeof(step_frames));
        std::memset(step_ms, 0, sizeof(step_ms));
        cur_md1 = 255;
        start_time = clock::now();
        last_change = start_time;
    }

    void record_md1(uint8_t md1, uint32_t sys)
    {
        if (!active) return;
        const auto now = clock::now();
        if (cur_md1 < 24) {
            step_ms[cur_md1] += std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();
        }
        last_change = now;
        cur_md1 = md1;
        if (md1 < 24) step_frames[md1]++;
        if (vm_read32(kFileStatAddr) != 0) wait_file_frames++;
        if (vm_read32(sys + kOffSdmFlg) & 1u) wait_snd_frames++;
        if (!(vm_read32(sys + kOffTsFlg) & 0x800u)) wait_door_flag_frames++;
        if (vm_read32(sys + kOffCbFlg) & 2u) wait_fade_frames++;
    }

    void finish(uint16_t room)
    {
        if (!active) return;
        const auto now = clock::now();
        if (cur_md1 < 24) {
            step_ms[cur_md1] += std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change).count();
        }
        const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        const int rcalls = g_door_read_calls.load(std::memory_order_relaxed);
        const long long rms = g_door_read_us.load(std::memory_order_relaxed) / 1000;
        const int icalls = g_door_inflate_calls.load(std::memory_order_relaxed);
        const long long ims = g_door_inflate_us.load(std::memory_order_relaxed) / 1000;
        char buf[1024];
        int n = std::snprintf(buf, sizeof(buf),
            "[cv_fast_door] room=0x%04X total=%lldms door_f=%d mon_f=%d (file=%df snd=%df reads=%d/%lldms inf=%d/%lldms) fast_fade=%d | steps:",
            (unsigned)room, (long long)total_ms, door_frames, monitor_frames,
            wait_file_frames, wait_snd_frames, rcalls, rms, icalls, ims,
            fast_fade_enabled() ? 1 : 0);
        for (int i = 0; i < 24 && n < (int)sizeof(buf) - 32; ++i) {
            if (step_frames[i] > 0 || step_ms[i] > 0) {
                n += std::snprintf(buf + n, sizeof(buf) - n, " s%d=%df/%lldms",
                    i, step_frames[i], step_ms[i]);
            }
        }
        log_line(buf);
        active = false;
    }
};

DoorProfiler g_prof;

} // namespace

/* Hook 1: bhSysCallDoordemo (EU 0x001AFAE8) */
void func_001AFAE8(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    const bool enabled = fast_door_enabled() && sys && !is_attract_demo(sys);

    if (!enabled) {
        cv_original_001AFAE8(ctx);
        return;
    }

    uint8_t mode0 = vm_read8(sys + kOffDoorMode0);

    if (mode0 == 0) {
        g_prof.begin();
        vm_write8(sys + kOffDoorMode0, 10u);
        mode0 = 10u;
    }

    if (g_prof.active) {
        g_prof.door_frames++;
    }

    if (mode0 == 10u && fast_fade_enabled()) {
        snap_active_fade(sys);
    }

    cv_original_001AFAE8(ctx);

    if (mode0 == 10u && vm_read8(sys + kOffDoorMode0) == 11u) {
        cv_original_001AFAE8(ctx);
    }
}

/* Hook 2: bhSysCallMonitor (EU 0x001B0CB0) */
void func_001B0CB0(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    const bool enabled = fast_door_enabled() && sys && !is_attract_demo(sys);

    if (!enabled) {
        cv_original_001B0CB0(ctx);
        return;
    }

    const uint8_t start_md0 = vm_read8(sys + kOffMnMd0);
    if (g_prof.active && start_md0 == 4u) {
        g_prof.monitor_frames++;
        g_prof.record_md1(vm_read8(sys + kOffMnMd1), sys);
    }

    static int micropoll_env = -1;
    if (micropoll_env < 0) micropoll_env = env_flag("CV_FAST_DOOR_MICROPOLL", true) ? 1 : 0;
    int wait_budget_ms = (micropoll_env && g_prof.active) ? 120 : 0;

    for (int pass = 0; pass < 64; ++pass) {
        const uint8_t before_md0 = vm_read8(sys + kOffMnMd0);
        const uint8_t before_md1 = vm_read8(sys + kOffMnMd1);

        if (before_md0 == 4u && before_md1 == 0u && fast_fade_enabled()) {
            snap_active_fade(sys);
        }

        if (before_md0 == 4u && before_md1 == 10u && g_prof.active) {
            if (!g_prof.stopped_bank9) {
                stop_sound_bank(ctx, 9u);
                g_prof.stopped_bank9 = true;
            } else if (!g_prof.stopped_bank10) {
                stop_sound_bank(ctx, 10u);
                g_prof.stopped_bank10 = true;
            }
        }

        cv_original_001B0CB0(ctx);

        const uint8_t after_md0 = vm_read8(sys + kOffMnMd0);
        const uint8_t after_md1 = vm_read8(sys + kOffMnMd1);

        if (before_md0 == 4u && after_md0 == 0u) {
            if (fast_fade_enabled()) {
                g_prof.fade_in_pending = true;
            }
            g_prof.finish(vm_read16(CV_SRT_ROOM));
            break;
        }

        if (after_md0 != 4u) {
            break;
        }

        if (after_md1 == 11u && before_md1 != 11u && (vm_read32(sys + kOffSdmFlg) & 0x1u)) {
            func_001B2A48(ctx);
        }

        if (after_md0 == before_md0 && after_md1 == before_md1) {
            /* Screen is already black waiting on background Resource Loader /
             * decodeProc threads (kFileStatAddr). Micro-poll in 1 ms steps so
             * file loading completes in ~5-30 ms in a single frame instead of
             * sleeping 33.3 ms per step. Never micro-poll s11 (snddrv), which
             * requires main-thread audio ticks to advance. */
            if (wait_budget_ms > 0 && vm_read32(kFileStatAddr) != 0) {
                while (wait_budget_ms > 0 && vm_read32(kFileStatAddr) != 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    --wait_budget_ms;
                }
                if (vm_read32(kFileStatAddr) == 0) continue;
            }
            break;
        }
    }
}

/* Hook 3: bhSysCallSndMonitor (EU 0x001B2A48) */
void func_001B2A48(ppu_context* ctx)
{
    const uint32_t sys = get_sys();
    const bool enabled = fast_door_enabled() && sys && !is_attract_demo(sys);

    if (!enabled) {
        cv_original_001B2A48(ctx);
        return;
    }

    if (g_prof.fade_in_pending && fast_fade_enabled()) {
        if (vm_read32(sys + kOffCbFlg) & 0x2u) {
            snap_active_fade(sys);
            g_prof.fade_in_pending = false;
        }
    }

    for (int pass = 0; pass < 16; ++pass) {
        const uint32_t before_flg  = vm_read32(sys + kOffSdmFlg);
        const uint32_t before_mode = vm_read32(sys + kOffSdmMode);

        cv_original_001B2A48(ctx);

        const uint32_t after_flg  = vm_read32(sys + kOffSdmFlg);
        const uint32_t after_mode = vm_read32(sys + kOffSdmMode);

        if (!(after_flg & 0x1u)) {
            break;
        }
        if (after_flg == before_flg && after_mode == before_mode) {
            break;
        }
    }
}

/* Hook 4a: MtFile::read (EU 0x007846D0) */
void func_007846D0(ppu_context* ctx)
{
    const uint32_t obj = (uint32_t)ctx->gpr[3];
    if (!fast_sync_read_enabled() || !obj) {
        cv_original_007846D0(ctx);
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const uint32_t flags = vm_read32(obj + 0x8);
    if ((flags & 4u) && vm_read8(obj + 0x42C) == 0) {
        const uint32_t pos = vm_read32(obj + 0x10);
        vm_write32(obj + 0x8, flags & ~4u);
        {
            GuestCall seek_call(ctx);
            seek_call.run(func_007858F4, obj, pos, 1u);
        }
        cv_original_007846D0(ctx);
        vm_write32(obj + 0x8, flags);
        vm_write32(obj + 0x424, (uint32_t)ctx->gpr[3]);
        vm_write8(obj + 0x42C, 0u);
    } else {
        cv_original_007846D0(ctx);
    }

    const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    g_door_read_calls.fetch_add(1, std::memory_order_relaxed);
    g_door_read_us.fetch_add(dt, std::memory_order_relaxed);
}

/* Hook 4b: MtFile::readAsync (EU 0x0078575C) */
void func_0078575C(ppu_context* ctx)
{
    cv_original_0078575C(ctx);
}

/* Hook 5a: decodeProc zlib stream ctor (EU 0x00538AEC) */
void func_00538AEC(ppu_context* ctx)
{
    const uint32_t stream_addr = (uint32_t)ctx->gpr[3];
    cv_original_00538AEC(ctx);
    if (native_zlib_enabled() && stream_addr) {
        zlib_slot_create(stream_addr);
    }
}

/* Hook 5b: decodeProc zlib stream dtor (EU 0x00539B0C) */
void func_00539B0C(ppu_context* ctx)
{
    const uint32_t stream_addr = (uint32_t)ctx->gpr[3];
    if (stream_addr) {
        zlib_slot_destroy(stream_addr);
    }
    cv_original_00539B0C(ctx);
}

/* Hook 5c: native x64 zlib inflate replacing SPU job at loc_00539EB0 */
extern "C" int cv_native_spu_inflate(ppu_context* ctx)
{
    if (!native_zlib_enabled() || !vm_base) return 0;

    const uint32_t stream_addr = (uint32_t)ctx->gpr[31];
    const uint32_t z_addr      = (uint32_t)ctx->gpr[30];
    NativeZlibSlot* slot = zlib_slot_find(stream_addr);
    if (!slot) return 0;

    const auto t0 = std::chrono::steady_clock::now();

    const uint32_t next_in   = vm_read32(z_addr + 0x00);
    const uint32_t avail_in  = vm_read32(z_addr + 0x04);
    const uint32_t total_in  = vm_read32(z_addr + 0x08);
    const uint32_t next_out  = vm_read32(z_addr + 0x0C);
    const uint32_t avail_out = vm_read32(z_addr + 0x10);
    const uint32_t total_out = vm_read32(z_addr + 0x14);
    const uint32_t copy_mode = vm_read32(stream_addr + 0xC8);

    z_stream& zs = slot->zs;
    zs.next_in  = vm_base + next_in;
    zs.avail_in = avail_in;

    int rc = Z_OK;
    uint32_t out_used = 0;

    /* Note: this+0xC8 == 0 is NORMAL read (write to next_out); != 0 is SKIP mode. */
    if (copy_mode == 0) {
        zs.next_out  = vm_base + next_out;
        zs.avail_out = avail_out;
        rc = ::inflate(&zs, Z_NO_FLUSH);
        out_used = avail_out - zs.avail_out;
        vm_write32(z_addr + 0x0C, next_out + out_used);
        vm_write32(z_addr + 0x10, zs.avail_out);
    } else {
        /* Skip/discard mode: inflate into stack scratch without writing to next_out. */
        uint8_t scratch[16384];
        uint32_t rem_out = avail_out;
        while (rem_out > 0 && zs.avail_in > 0) {
            const uint32_t chunk = rem_out < sizeof(scratch) ? rem_out : (uint32_t)sizeof(scratch);
            zs.next_out  = scratch;
            zs.avail_out = chunk;
            rc = ::inflate(&zs, Z_NO_FLUSH);
            const uint32_t got = chunk - zs.avail_out;
            out_used += got;
            rem_out  -= got;
            if (rc != Z_OK || (got == 0 && zs.avail_in == 0)) break;
        }
        vm_write32(z_addr + 0x10, rem_out);
    }

    const uint32_t in_used = avail_in - zs.avail_in;
    vm_write32(z_addr + 0x00, next_in + in_used);
    vm_write32(z_addr + 0x04, zs.avail_in);
    vm_write32(z_addr + 0x08, total_in + in_used);
    vm_write32(z_addr + 0x14, total_out + out_used);
    vm_write32(stream_addr + 0xB8, (uint32_t)rc);

    const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    g_door_inflate_calls.fetch_add(1, std::memory_order_relaxed);
    g_door_inflate_us.fetch_add(dt, std::memory_order_relaxed);
    return 1;
}

/* Hook 6: njCalcPoint4 (EU 0x001CF5D4) -- hot leaf mat4x4 * vec3 -> vec4 */
void func_001CF5D4(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_001CF5D4(ctx);
        return;
    }

    uint32_t mat = (uint32_t)ctx->gpr[3];
    const uint32_t src = (uint32_t)ctx->gpr[4];
    const uint32_t dst = (uint32_t)ctx->gpr[5];
    if (!mat) {
        mat = read_be32_fast(vm_base + 0x00ABED90u); /* (0xAC << 16) - 0x1270 */
    }

    const uint8_t* s = vm_base + src;
    const uint8_t* m = vm_base + mat;
    uint8_t* d = vm_base + dst;

    const float x = read_bef32_fast(s + 0x0);
    const float y = read_bef32_fast(s + 0x4);
    const float z = read_bef32_fast(s + 0x8);

    const float ox = read_bef32_fast(m + 0x00) * x + read_bef32_fast(m + 0x10) * y + read_bef32_fast(m + 0x20) * z + read_bef32_fast(m + 0x30);
    const float oy = read_bef32_fast(m + 0x04) * x + read_bef32_fast(m + 0x14) * y + read_bef32_fast(m + 0x24) * z + read_bef32_fast(m + 0x34);
    const float oz = read_bef32_fast(m + 0x08) * x + read_bef32_fast(m + 0x18) * y + read_bef32_fast(m + 0x28) * z + read_bef32_fast(m + 0x38);
    const float ow = read_bef32_fast(m + 0x0C) * x + read_bef32_fast(m + 0x1C) * y + read_bef32_fast(m + 0x2C) * z + read_bef32_fast(m + 0x3C);

    write_bef32_fast(d + 0x0, ox);
    write_bef32_fast(d + 0x4, oy);
    write_bef32_fast(d + 0x8, oz);
    write_bef32_fast(d + 0xC, ow);
}

/* Hook 7: K-way draw-call merge sort (EU 0x00287750) -- hot leaf on 1000+ draw rooms */
void func_00287750(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_00287750(ctx);
        return;
    }

    const uint32_t desc = (uint32_t)ctx->gpr[4];
    const uint32_t lists_addr = read_be32_fast(vm_base + desc + 0x0);
    const uint32_t count      = read_be32_fast(vm_base + desc + 0x4);
    const uint32_t out_addr   = read_be32_fast(vm_base + desc + 0x8);

    if (count == 0) return;
    if (count > 32) {
        cv_original_00287750(ctx);
        return;
    }

    const uint8_t* ptrs[32];
    uint32_t lens[32];
    uint32_t idxs[32];
    uint32_t total = 0;

    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* entry = vm_base + lists_addr + i * 12u;
        lens[i] = read_be32_fast(entry + 0x4);
        ptrs[i] = vm_base + read_be32_fast(entry + 0x8);
        idxs[i] = 0;
        total += lens[i];
    }

    if (total == 0) return;

    uint8_t* out = vm_base + out_addr;

    if (count == 1) {
        std::memcpy(out, ptrs[0], (size_t)total * 8u);
        return;
    }

    for (uint32_t k = 0; k < total; ++k) {
        uint32_t best_key = 0xFFFFFFFFu;
        uint32_t best_i = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (idxs[i] < lens[i]) {
                const uint32_t key = read_be32_fast(ptrs[i] + (size_t)idxs[i] * 8u);
                if (best_key > key) {
                    best_key = key;
                    best_i = i;
                }
            }
        }
        std::memcpy(out + (size_t)k * 8u, ptrs[best_i] + (size_t)idxs[best_i] * 8u, 8u);
        idxs[best_i]++;
    }
}

/* Hook 8: njMultiMatrix affine 3x4 (EU 0x001CE930) -- 3.8% of guest CPU */
void func_001CE930(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_001CE930(ctx);
        return;
    }

    uint32_t dst_addr = (uint32_t)ctx->gpr[3];
    const uint32_t src_addr = (uint32_t)ctx->gpr[4];
    if (!dst_addr) {
        dst_addr = read_be32_fast(vm_base + 0x00ABED90u);
    }

    uint8_t* a = vm_base + dst_addr;
    const uint8_t* b = vm_base + src_addr;

    const float a00 = read_bef32_fast(a + 0x00), a01 = read_bef32_fast(a + 0x04), a02 = read_bef32_fast(a + 0x08);
    const float a10 = read_bef32_fast(a + 0x10), a11 = read_bef32_fast(a + 0x14), a12 = read_bef32_fast(a + 0x18);
    const float a20 = read_bef32_fast(a + 0x20), a21 = read_bef32_fast(a + 0x24), a22 = read_bef32_fast(a + 0x28);
    const float a30 = read_bef32_fast(a + 0x30), a31 = read_bef32_fast(a + 0x34), a32 = read_bef32_fast(a + 0x38);

    const float b00 = read_bef32_fast(b + 0x00), b01 = read_bef32_fast(b + 0x04), b02 = read_bef32_fast(b + 0x08);
    const float b10 = read_bef32_fast(b + 0x10), b11 = read_bef32_fast(b + 0x14), b12 = read_bef32_fast(b + 0x18);
    const float b20 = read_bef32_fast(b + 0x20), b21 = read_bef32_fast(b + 0x24), b22 = read_bef32_fast(b + 0x28);
    const float b30 = read_bef32_fast(b + 0x30), b31 = read_bef32_fast(b + 0x34), b32 = read_bef32_fast(b + 0x38);

    write_bef32_fast(a + 0x30, a00 * b30 + a10 * b31 + a20 * b32 + a30);
    write_bef32_fast(a + 0x34, a01 * b30 + a11 * b31 + a21 * b32 + a31);
    write_bef32_fast(a + 0x38, a02 * b30 + a12 * b31 + a22 * b32 + a32);

    write_bef32_fast(a + 0x00, a00 * b00 + a10 * b01 + a20 * b02);
    write_bef32_fast(a + 0x04, a01 * b00 + a11 * b01 + a21 * b02);
    write_bef32_fast(a + 0x08, a02 * b00 + a12 * b01 + a22 * b02);

    write_bef32_fast(a + 0x10, a00 * b10 + a10 * b11 + a20 * b12);
    write_bef32_fast(a + 0x14, a01 * b10 + a11 * b11 + a21 * b12);
    write_bef32_fast(a + 0x18, a02 * b10 + a12 * b11 + a22 * b12);

    write_bef32_fast(a + 0x20, a00 * b20 + a10 * b21 + a20 * b22);
    write_bef32_fast(a + 0x24, a01 * b20 + a11 * b21 + a21 * b22);
    write_bef32_fast(a + 0x28, a02 * b20 + a12 * b21 + a22 * b22);
}

/* Hook 15 helper: atomic scratch bump allocator (EU 0x000124F4) -- 3.3% of guest CPU */
static inline uint32_t cv_alloc_scratch_000124F4(uint32_t pool_idx, uint32_t size)
{
    const uint32_t mgr = read_be32_fast(vm_base + 0x00A83C68u); /* (0xA8<<16) + 0x3C68 */
    if (!mgr) return 0u;

    const uint32_t pool    = mgr + pool_idx * 24u;
    const uint32_t aligned = (size + 0xFu) & ~0xFu;
    const uint32_t cap     = read_be32_fast(vm_base + pool + 0x2Cu);
    const uint32_t cur0    = read_be32_fast(vm_base + pool + 0x30u);
    if (cur0 + aligned >= cap) return 0u;

    std::atomic_ref<uint32_t> cur_atom(*reinterpret_cast<uint32_t*>(vm_base + pool + 0x30u));
    uint32_t cur_be = cur_atom.load(std::memory_order_relaxed);
    uint32_t old_cur;
    for (;;) {
        old_cur = bswap32_fast(cur_be);
        const uint32_t next_be = bswap32_fast(old_cur + aligned);
        if (cur_atom.compare_exchange_weak(cur_be, next_be,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
            break;
        }
    }

    const uint32_t cnt = read_be32_fast(vm_base + pool + 0x34u);
    write_be32_fast(vm_base + pool + 0x34u, cnt + 1u);
    const uint32_t base = read_be32_fast(vm_base + pool + 0x38u);
    return base + old_cur;
}

void func_000124F4(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_000124F4(ctx);
        return;
    }
    ctx->gpr[3] = cv_alloc_scratch_000124F4((uint32_t)ctx->gpr[3], (uint32_t)ctx->gpr[4]);
}

/* Hook 9: vertex + normal + bbox transform loop (EU 0x001D32F0) -- 6.6% of guest CPU */
void func_001D32F0(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_001D32F0(ctx);
        return;
    }

    const uint32_t mesh = (uint32_t)ctx->gpr[3];
    const uint32_t count = vm_read16(mesh + 0x6);
    const uint32_t ctx_obj = read_be32_fast(vm_base + 0x00ABEDFCu); /* (0xAC << 16) - 0x1204 */
    if (!ctx_obj) {
        ctx->gpr[3] = mesh + 0x40u + (count << 5);
        return;
    }

    uint32_t pos_buf = cv_alloc_scratch_000124F4(0u, count << 4);
    write_be32_fast(vm_base + ctx_obj + 0x04u, pos_buf);
    uint32_t col_buf = cv_alloc_scratch_000124F4(0u, count << 2);
    write_be32_fast(vm_base + ctx_obj + 0x08u, col_buf);
    write_be32_fast(vm_base + ctx_obj + 0x10u, count);
    uint32_t norm_buf = 0u;
    if (count != 0) {
        norm_buf = cv_alloc_scratch_000124F4(0u, count << 4);
        write_be32_fast(vm_base + ctx_obj + 0x0Cu, norm_buf);
        write_be32_fast(vm_base + ctx_obj + 0x14u, count);
    }

    if (count != 0) {
        uint8_t def_col_be[4];
        std::memcpy(def_col_be, vm_base + 0x00BB13ECu, 4);
        uint8_t* col_p = vm_base + col_buf;
        for (uint32_t i = 0; i < count; ++i) {
            std::memcpy(col_p + i * 4u, def_col_be, 4);
        }
    }

    vm_write8(ctx_obj + 0x18, 0u);

    if (count != 0) {
        const uint32_t mat_addr = read_be32_fast(vm_base + 0x00ABED90u);
        const uint8_t* m = vm_base + mat_addr;
        const float m00 = read_bef32_fast(m + 0x00), m01 = read_bef32_fast(m + 0x04), m02 = read_bef32_fast(m + 0x08);
        const float m10 = read_bef32_fast(m + 0x10), m11 = read_bef32_fast(m + 0x14), m12 = read_bef32_fast(m + 0x18);
        const float m20 = read_bef32_fast(m + 0x20), m21 = read_bef32_fast(m + 0x24), m22 = read_bef32_fast(m + 0x28);
        const float m30 = read_bef32_fast(m + 0x30), m31 = read_bef32_fast(m + 0x34), m32 = read_bef32_fast(m + 0x38);

        const bool track_bbox = (read_be32_fast(vm_base + 0x00AECEA0u) == 1u);
        float bmin[4] = {}, bmax[4] = {};
        uint32_t bcnt = 0;
        if (track_bbox) {
            for (int k = 0; k < 4; ++k) {
                bmin[k] = read_bef32_fast(vm_base + 0x00AECEB0u + k * 4u);
                bmax[k] = read_bef32_fast(vm_base + 0x00AECEC0u + k * 4u);
            }
            bcnt = read_be32_fast(vm_base + 0x00AECEA4u);
        }

        const uint8_t* vsrc = vm_base + mesh + 0x40u;
        uint8_t* pdst = vm_base + pos_buf;
        uint8_t* ndst = vm_base + norm_buf;

        const bool flog = frustum_log_enabled() && (read_be32_fast(vm_base + 0x00AECE78u) == 0u);
        uint32_t out_and = flog ? 0x3Fu : 0u;
        const float f_zn = flog ? read_bef32_fast(vm_base + 0x00CB7350u) : 0.0f;
        const float f_zf = flog ? read_bef32_fast(vm_base + 0x00CB7354u) : 0.0f;
        const float f_cx = flog ? read_bef32_fast(vm_base + 0x00CB7358u) * 1.15f : 0.0f;
        const float f_cy = flog ? read_bef32_fast(vm_base + 0x00CB735Cu) * 1.15f : 0.0f;
        const float f_pd = flog ? read_bef32_fast(vm_base + 0x00ADCB10u) : 0.0f;

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* v = vsrc + i * 0x20u;
            const float x = read_bef32_fast(v + 0x00);
            const float y = read_bef32_fast(v + 0x04);
            const float z = read_bef32_fast(v + 0x08);

            const float px = m00 * x + m10 * y + m20 * z + m30;
            const float py = m01 * x + m11 * y + m21 * z + m31;
            const float pz = m02 * x + m12 * y + m22 * z + m32;

            if (out_and) {
                uint32_t oc = 0u;
                if (pz < f_zn) oc |= 1u;
                if (pz > f_zf) oc |= 2u;
                if (std::fabs(pz) >= 1e-4f) {
                    const float inv_z = std::fabs(f_pd / pz);
                    const float sx = px * inv_z;
                    const float sy = py * inv_z;
                    if (sx < -f_cx) oc |= 4u;
                    if (sx >  f_cx) oc |= 8u;
                    if (sy < -f_cy) oc |= 16u;
                    if (sy >  f_cy) oc |= 32u;
                }
                out_and &= oc;
            }

            uint8_t* po = pdst + i * 0x10u;
            write_bef32_fast(po + 0x0, px);
            write_bef32_fast(po + 0x4, py);
            write_bef32_fast(po + 0x8, pz);

            if (track_bbox) {
                if (px < bmin[0]) bmin[0] = px;
                if (py < bmin[1]) bmin[1] = py;
                if (pz < bmin[2]) bmin[2] = pz;
                if (0.0f < bmin[3]) bmin[3] = 0.0f;
                if (px > bmax[0]) bmax[0] = px;
                if (py > bmax[1]) bmax[1] = py;
                if (pz > bmax[2]) bmax[2] = pz;
                if (0.0f > bmax[3]) bmax[3] = 0.0f;
                bcnt++;
            }

            const float nx = read_bef32_fast(v + 0x10);
            const float ny = read_bef32_fast(v + 0x14);
            const float nz = read_bef32_fast(v + 0x18);

            const float tnx = m00 * nx + m10 * ny + m20 * nz;
            const float tny = m01 * nx + m11 * ny + m21 * nz;
            const float tnz = m02 * nx + m12 * ny + m22 * nz;

            uint8_t* no = ndst + i * 0x10u;
            write_bef32_fast(no + 0x0, tnx);
            write_bef32_fast(no + 0x4, tny);
            write_bef32_fast(no + 0x8, tnz);
        }

        if (flog) {
            cv_frustum_record_32f0(count, out_and != 0u);
        }

        if (track_bbox) {
            for (int k = 0; k < 4; ++k) {
                write_bef32_fast(vm_base + 0x00AECEB0u + k * 4u, bmin[k]);
                write_bef32_fast(vm_base + 0x00AECEC0u + k * 4u, bmax[k]);
            }
            vm_write32(0x00AECEA4u, bcnt);
        }
    }

    ctx->gpr[3] = mesh + 0x40u + (count << 5);
}

/* Hook 10: UI/draw command vector push_back fast path (EU 0x00015AFC) -- 4.0%-6.8% of guest CPU */
void cv_fast_enqueue_00015AFC(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_ui_original_enqueue(ctx);
        return;
    }

    const uint32_t obj  = (uint32_t)ctx->gpr[3];
    const uint32_t item = (uint32_t)ctx->gpr[4];
    const int32_t mode  = (int32_t)read_be32_fast(vm_base + obj + 0x14Cu);

    uint32_t off_cnt, off_cap, off_arr;
    if (mode == 3 || mode == 4) {
        off_cnt = 0xECu;
        off_cap = 0xF0u;
        off_arr = 0xF8u;
    } else if ((read_be32_fast(vm_base + item + 0x10u) & 0x100u) != 0u) {
        off_cnt = 0xD8u;
        off_cap = 0xDCu;
        off_arr = 0xE4u;
    } else {
        off_cnt = 0xC4u;
        off_cap = 0xC8u;
        off_arr = 0xD0u;
    }

    const uint32_t cnt = read_be32_fast(vm_base + obj + off_cnt);
    const uint32_t cap = read_be32_fast(vm_base + obj + off_cap);
    if (cnt < cap) {
        const uint32_t arr = read_be32_fast(vm_base + obj + off_arr);
        write_be32_fast(vm_base + obj + off_cnt, cnt + 1u);
        write_be32_fast(vm_base + arr + (cnt << 2), item);
        return;
    }

    /* Slow path: vector growth / realloc (rare) */
    cv_ui_original_enqueue(ctx);
}

/* Hook 11: vertex buffer + draw descriptor fast allocator (EU 0x0027EF90) -- 4.0% of guest CPU */
void func_0027EF90(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_0027EF90(ctx);
        return;
    }

    const uint32_t obj       = (uint32_t)ctx->gpr[3];
    const uint32_t vtx_count = (uint32_t)ctx->gpr[4];
    const uint32_t stride    = (uint32_t)ctx->gpr[5];

    if ((read_be32_fast(vm_base + obj + 0x4Cu) & 0x50u) != 0u) {
        ctx->gpr[3] = 0;
        return;
    }

    const uint32_t bytes = (uint32_t)((int32_t)stride * (int32_t)vtx_count);
    const uint32_t vcur  = read_be32_fast(vm_base + obj + 0x1Cu);
    const uint32_t vend  = read_be32_fast(vm_base + obj + 0x24u);
    if (vcur + bytes > vend || vcur == 0u) {
        cv_original_0027EF90(ctx);
        return;
    }

    if ((uint32_t)vm_base[obj + 0x74u] != stride) {
        cv_original_0027EF90(ctx);
        return;
    }

    const uint32_t dcur = read_be32_fast(vm_base + obj + 0x14u);
    const uint32_t dend = read_be32_fast(vm_base + obj + 0x18u);
    if (dcur + 0x20u > dend) {
        cv_original_0027EF90(ctx);
        return;
    }

    /* Fast path: bump vertex & descriptor cursors, write 32-byte desc + 8-byte sort entry */
    write_be32_fast(vm_base + obj + 0x1Cu, vcur + bytes);
    write_be32_fast(vm_base + obj + 0x14u, dcur + 0x20u);

    uint8_t* d = vm_base + dcur;
    const uint32_t w0 = (read_be32_fast(d + 0x0) & 0xFFFF0000u)
                      | (read_be32_fast(vm_base + obj + 0x5Cu) & 0x00000FFFu);
    write_be32_fast(d + 0x00u, w0);
    write_be32_fast(d + 0x04u, read_be32_fast(vm_base + obj + 0x1CCu));
    write_be32_fast(d + 0x08u, read_be32_fast(vm_base + obj + 0x1C0u));
    write_be32_fast(d + 0x0Cu, read_be32_fast(vm_base + obj + 0x1C4u));
    write_be32_fast(d + 0x10u, read_be32_fast(vm_base + obj + 0xB8u));
    write_be32_fast(d + 0x14u, vtx_count);
    const uint32_t vtx_off = read_be32_fast(vm_base + obj + 0xB4u);
    write_be32_fast(d + 0x18u, vtx_off);
    write_be32_fast(d + 0x1Cu, 0u);
    write_be32_fast(vm_base + obj + 0xB4u, vtx_off + vtx_count);

    const uint32_t sort_idx  = read_be32_fast(vm_base + obj + 0x38u);
    const uint32_t sort_base = read_be32_fast(vm_base + obj + 0x34u);
    write_be32_fast(vm_base + obj + 0x38u, sort_idx + 1u);

    const uint32_t key_base = read_be32_fast(vm_base + obj + 0x194u);
    const uint32_t key_add  = read_be32_fast(vm_base + obj + 0x1A8u);
    const uint32_t sort_key = (key_base & 0xF8000000u)
                            | (((key_base & 0x07FFFFFFu) + key_add) & 0x07FFFFFFu);
    uint8_t* s = vm_base + sort_base + (sort_idx << 3);
    write_be32_fast(s + 0x0u, sort_key);
    write_be32_fast(s + 0x4u, dcur);

    ctx->gpr[3] = vcur;
}

/* Hook 12: 2-way bottom-up stable merge sort of 8-byte (u32 key, u32 val) pairs (EU 0x00286E74) -- 4.1% of guest CPU */
static inline uint32_t be_key_from_pair64(uint64_t entry)
{
#ifdef _MSC_VER
    return _byteswap_ulong((uint32_t)entry);
#else
    return __builtin_bswap32((uint32_t)entry);
#endif
}

void func_00286E74(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_00286E74(ctx);
        return;
    }

    const uint32_t arr_in  = (uint32_t)ctx->gpr[4];
    const int32_t  count   = (int32_t)ctx->gpr[5];
    const uint32_t arr_tmp = (uint32_t)ctx->gpr[6];

    int32_t pow2 = 1;
    for (int i = 0; i < 32 && count > pow2; ++i) {
        pow2 <<= 1;
    }
    if (pow2 < 2) {
        ctx->gpr[3] = arr_in;
        return;
    }

    uint64_t* a_in  = reinterpret_cast<uint64_t*>(vm_base + arr_in);
    uint64_t* a_tmp = reinterpret_cast<uint64_t*>(vm_base + arr_tmp);

    /* Pass 1: sort pairs (run = 2) from arr_in -> arr_tmp */
    for (int32_t i = 0; i < count; i += 2) {
        if (i + 1 >= count) {
            a_tmp[i] = a_in[i];
        } else {
            const uint64_t e0 = a_in[i];
            const uint64_t e1 = a_in[i + 1];
            if (be_key_from_pair64(e0) > be_key_from_pair64(e1)) {
                a_tmp[i]     = e1;
                a_tmp[i + 1] = e0;
            } else {
                a_tmp[i]     = e0;
                a_tmp[i + 1] = e1;
            }
        }
    }

    int parity = 1;
    for (int32_t run = 4; run <= pow2; run <<= 1) {
        const uint64_t* src = parity ? a_tmp : a_in;
        uint64_t*       dst = parity ? a_in  : a_tmp;
        const int32_t half = run >> 1;

        for (int32_t start = 0; start < count; start += run) {
            int32_t mid = start + half;
            if (mid > count) mid = count;
            int32_t end = mid + half;
            if (end > count) end = count;

            int32_t l = start, r = mid, o = start;
            while (l < mid && r < end) {
                const uint64_t el = src[l];
                const uint64_t er = src[r];
                if (be_key_from_pair64(el) > be_key_from_pair64(er)) {
                    dst[o++] = er;
                    ++r;
                } else {
                    dst[o++] = el;
                    ++l;
                }
            }
            if (l < mid) {
                std::memcpy(dst + o, src + l, (size_t)(mid - l) * 8u);
            } else if (r < end) {
                std::memcpy(dst + o, src + r, (size_t)(end - r) * 8u);
            }
        }
        parity ^= 1;
    }

    ctx->gpr[3] = parity ? arr_tmp : arr_in;
}

/* Hook 13: MT Framework vertex attribute packer (EU 0x004BAAC0) -- 6.9% (#1) of guest CPU */
static inline float cv_clampf(float v, float lo, float hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static inline uint16_t cv_float32_to_half(uint32_t bits)
{
    const int32_t  exp  = (int32_t)((bits >> 23) & 0xFFu) - 112;
    const uint32_t mant = bits & 0x007FFFFFu;
    uint16_t h = (uint16_t)((bits >> 16) & 0x8000u);

    if (exp <= 0) {
        if (exp < -10) return 0u;
        const uint32_t m = (mant | 0x00800000u) >> (1 - exp);
        return (uint16_t)(h | (m >> 13));
    }
    if (exp == 0x8F) {
        if (mant == 0u) return (uint16_t)(h | 0x7C00u);
        const uint16_t q = (uint16_t)((bits >> 13) & 0x3FFu);
        return (uint16_t)(h | 0x7C00u | q | (q == 0u ? 1u : 0u));
    }
    if (exp > 0x1E) return (uint16_t)(h | 0x7C00u);
    return (uint16_t)(h | ((uint32_t)exp << 10) | ((bits >> 13) & 0x3FFu));
}

template <typename ReadFloatFn>
static inline uint32_t cv_pack_vertex_attr_impl(uint8_t* dst_base, uint32_t desc, ReadFloatFn read_src_f)
{
    const uint32_t dst_off = (desc >> 1) & 0x1FFu;
    const uint32_t fmt     = (desc >> 21) & 0x1Fu;
    const uint32_t count   = (desc >> 14) & 0x7Fu;
    const uint32_t sbase   = (desc >> 10) & 0x0Fu;
    uint8_t* dst = dst_base + dst_off;
    const uint8_t* ctab = vm_base + 0x004BAA78u; /* (0x4C << 16) - 21896 */

    switch (fmt) {
    case 1: /* Float32 */
        for (uint32_t i = 0; i < count; ++i) {
            write_bef32_fast(dst + i * 4u, read_src_f(sbase + i));
        }
        return count;

    case 2: /* Float16 */
        for (uint32_t i = 0; i < count; ++i) {
            const float f = read_src_f(sbase + i);
            uint32_t bits;
            std::memcpy(&bits, &f, 4);
            write_be16_fast(dst + i * 2u, cv_float32_to_half(bits));
        }
        return count;

    case 3: { /* Int16 clamped */
        if (count == 0u) return 0u;
        const float lo = read_bef32_fast(ctab + 0x00u);
        const float hi = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi);
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 4: { /* Int16 (f - c08) clamped */
        if (count == 0u) return 0u;
        const float lo   = read_bef32_fast(ctab + 0x00u);
        const float hi   = read_bef32_fast(ctab + 0x04u);
        const float bias = read_bef32_fast(ctab + 0x08u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) - bias, lo, hi);
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 5: { /* Snorm16: clamp(f, c0C, c10) * c04 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x0Cu);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi) * scale;
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 6: { /* Snorm16 biased: clamp((f - c14) * c18, c0C, c10) * c04 */
        if (count == 0u) return 0u;
        const float c14   = read_bef32_fast(ctab + 0x14u);
        const float c18   = read_bef32_fast(ctab + 0x18u);
        const float lo    = read_bef32_fast(ctab + 0x0Cu);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x04u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf((read_src_f(sbase + i) - c14) * c18, lo, hi) * scale;
            write_be16_fast(dst + i * 2u, (uint16_t)(int32_t)f);
        }
        return count;
    }

    case 7: { /* Uint8: clamp(f + c1C, c20, c24) */
        if (count == 0u) return 0u;
        const float bias = read_bef32_fast(ctab + 0x1Cu);
        const float lo   = read_bef32_fast(ctab + 0x20u);
        const float hi   = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) + bias, lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 8: { /* Uint8: clamp(f, c20, c24) */
        if (count == 0u) return 0u;
        const float lo = read_bef32_fast(ctab + 0x20u);
        const float hi = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 9: { /* Uint8: clamp(f * c40 + c40, c20, c24) */
        if (count == 0u) return 0u;
        const float c40 = read_bef32_fast(ctab + 0x40u);
        const float lo  = read_bef32_fast(ctab + 0x20u);
        const float hi  = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i) * c40 + c40, lo, hi);
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 10: { /* Unorm8: clamp(f, c20, c10) * c24 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float f = cv_clampf(read_src_f(sbase + i), lo, hi) * scale;
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 11: { /* Packed 3-comp signed -> RGBA8 (r<<24 | g<<16 | b<<8 | 0xFF) */
        if (count == 0u) return 0u;
        const float lo  = read_bef32_fast(ctab + 0x0Cu);
        const float hi  = read_bef32_fast(ctab + 0x10u);
        const float c40 = read_bef32_fast(ctab + 0x40u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 3u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * c40 + c40);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * c40 + c40);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * c40 + c40);
            write_be32_fast(dst + i * 4u, (b0 << 24) | (b1 << 16) | (b2 << 8) | 0xFFu);
        }
        return count * 3u;
    }

    case 12: { /* Packed 3-comp unorm -> 0xFF000000 | (b2<<16) | (b1<<8) | b0 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 3u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * scale);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * scale);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * scale);
            write_be32_fast(dst + i * 4u, 0xFF000000u | (b2 << 16) | (b1 << 8) | b0);
        }
        return count * 3u;
    }

    case 13: { /* Uint8 Reinhard: clamp(f / (f + c10), c20, c10) * c24 */
        if (count == 0u) return 0u;
        const float c10   = read_bef32_fast(ctab + 0x10u);
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const float x = read_src_f(sbase + i);
            const float f = cv_clampf(x / (x + c10), lo, c10) * scale;
            dst[i] = (uint8_t)(int64_t)f;
        }
        return count;
    }

    case 14: { /* Packed 4-comp unorm -> (b3<<24) | (b2<<16) | (b1<<8) | b0 */
        if (count == 0u) return 0u;
        const float lo    = read_bef32_fast(ctab + 0x20u);
        const float hi    = read_bef32_fast(ctab + 0x10u);
        const float scale = read_bef32_fast(ctab + 0x24u);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t idx = (sbase + i) * 4u;
            const uint32_t b0 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 0u), lo, hi) * scale);
            const uint32_t b1 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 1u), lo, hi) * scale);
            const uint32_t b2 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 2u), lo, hi) * scale);
            const uint32_t b3 = (uint32_t)(uint8_t)(int64_t)(cv_clampf(read_src_f(idx + 3u), lo, hi) * scale);
            write_be32_fast(dst + i * 4u, (b3 << 24) | (b2 << 16) | (b1 << 8) | b0);
        }
        return count * 4u;
    }

    default:
        return 0u;
    }
}

void func_004BAAC0(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_004BAAC0(ctx);
        return;
    }

    const uint32_t dst_addr  = (uint32_t)ctx->gpr[3];
    const uint32_t desc_addr = (uint32_t)ctx->gpr[4];
    const uint32_t src_addr  = (uint32_t)ctx->gpr[5];
    const uint32_t desc      = read_be32_fast(vm_base + desc_addr + 0x4u);
    const uint8_t* src_ptr   = vm_base + src_addr;

    ctx->gpr[3] = cv_pack_vertex_attr_impl(
        vm_base + dst_addr, desc,
        [src_ptr](uint32_t idx) -> float { return read_bef32_fast(src_ptr + idx * 4u); });
}

/* Hook 14: Vertex buffer builder (EU 0x0001D5D0) -- 4.3% of guest CPU, inlines func_004BAAC0 */
void func_0001D5D0(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_0001D5D0(ctx);
        return;
    }

    const uint32_t geom_hdr = (uint32_t)ctx->gpr[4] + 4u;
    const uint32_t mesh     = (uint32_t)ctx->gpr[5];

    /* 1. Free existing vertex buffer if present */
    const uint32_t old_buf = read_be32_fast(vm_base + mesh + 0x34u);
    if (old_buf != 0u) {
        const uint32_t alloc = read_be32_fast(vm_base + 0x00B40FCCu + 0x20u);
        const uint32_t vt    = read_be32_fast(vm_base + alloc);
        const uint32_t fn    = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x28u));
        GuestCall call(ctx);
        call.run_indirect_u32(fn, alloc, old_buf);
        write_be32_fast(vm_base + mesh + 0x34u, 0u);
    }

    /* 2. Compute total vertex buffer size */
    const uint32_t num_groups  = read_be32_fast(vm_base + mesh + 0x04u);
    const uint32_t global_obj  = read_be32_fast(vm_base + 0x00AFBDDCu); /* (0xB0<<16) - 0x4224 */
    const uint32_t vfmt        = read_be32_fast(vm_base + global_obj + 0x208u);
    const uint32_t vtx_stride  = read_be32_fast(vm_base + vfmt + 0x20u);
    const bool     has_normals = (read_be32_fast(vm_base + geom_hdr + 0x10u) != 0u);
    const uint32_t prim_type   = read_be32_fast(vm_base + mesh + 0x3Cu);
    const uint32_t groups_base = read_be32_fast(vm_base + mesh + 0x28u);

    uint32_t total_bytes = 0u;
    for (uint32_t g = 0; g < num_groups; ++g) {
        uint32_t n = read_be32_fast(vm_base + groups_base + g * 0x10u);
        if (prim_type == 3u) {
            n = n * 3u - 6u;
        }
        total_bytes += n * vtx_stride;
    }

    /* 3. Allocate new 16-byte-aligned vertex buffer */
    uint32_t dst_vbuf = 0u;
    {
        const uint32_t alloc = read_be32_fast(vm_base + 0x00B40FCCu + 0x20u);
        const uint32_t vt    = read_be32_fast(vm_base + alloc);
        const uint32_t fn    = read_be32_fast(vm_base + read_be32_fast(vm_base + vt + 0x1Cu));
        GuestCall call(ctx);
        dst_vbuf = call.run_indirect_u32(fn, alloc, total_bytes, 0x10u);
    }
    write_be32_fast(vm_base + mesh + 0x34u, dst_vbuf);

    if (num_groups == 0u || !dst_vbuf) return;

    const uint32_t attr_descs = read_be32_fast(vm_base + vfmt + 0x18u);
    const uint32_t desc_pos   = read_be32_fast(vm_base + attr_descs + 0x04u);
    const uint32_t desc_nrm   = read_be32_fast(vm_base + attr_descs + 0x0Cu);
    const uint32_t desc_uv    = read_be32_fast(vm_base + attr_descs + 0x14u);

    const uint32_t pos_base = read_be32_fast(vm_base + geom_hdr + 0x00u);
    const uint32_t nrm_base = read_be32_fast(vm_base + geom_hdr + 0x08u);
    const uint8_t* def_nrm_p = vm_base + 0x00BB9850u; /* (0xBC<<16) - 26544 */
    const float def_nrm[4] = {
        read_bef32_fast(def_nrm_p + 0x0u), read_bef32_fast(def_nrm_p + 0x4u),
        read_bef32_fast(def_nrm_p + 0x8u), read_bef32_fast(def_nrm_p + 0xCu)
    };
    const uint8_t* def_uv_p = vm_base + 0x00BB96ECu; /* (0xBC<<16) - 26900 */
    const float def_uv[2] = {
        read_bef32_fast(def_uv_p + 0x0u), read_bef32_fast(def_uv_p + 0x4u)
    };

    uint8_t* out_p = vm_base + dst_vbuf;

    if (prim_type == 3u) {
        /* Triangle strip -> triangle list */
        for (uint32_t g = 0; g < num_groups; ++g) {
            const uint32_t grp_addr     = groups_base + g * 0x10u;
            const uint32_t n_verts      = read_be32_fast(vm_base + grp_addr + 0x0u);
            const uint8_t  flip_winding = vm_base[grp_addr + 0x4u];
            const uint8_t  has_uv       = vm_base[grp_addr + 0x5u];
            const uint32_t idx_arr      = read_be32_fast(vm_base + grp_addr + 0x8u);
            const uint32_t uv_arr       = read_be32_fast(vm_base + grp_addr + 0xCu);
            const uint32_t n_tris       = n_verts - 2u;

            for (uint32_t t = 0; t < n_tris; ++t) {
                const bool rev = ((flip_winding != 0u) ^ ((t & 1u) != 0u));
                for (uint32_t k = 0; k < 3u; ++k) {
                    const uint32_t vi   = rev ? (t + 2u - k) : (t + k);
                    const uint32_t vidx = (vi < n_verts && idx_arr)
                                        ? read_be32_fast(vm_base + idx_arr + vi * 4u)
                                        : read_be32_fast(vm_base);

                    const uint8_t* pp = vm_base + ((pos_base + (vidx << 4)) & ~0xFu);
                    const float pos[4] = {
                        read_bef32_fast(pp + 0x0u), read_bef32_fast(pp + 0x4u),
                        read_bef32_fast(pp + 0x8u), read_bef32_fast(pp + 0xCu)
                    };

                    float nrm[4];
                    if (has_normals) {
                        const uint8_t* np = vm_base + ((nrm_base + (vidx << 4)) & ~0xFu);
                        nrm[0] = read_bef32_fast(np + 0x0u);
                        nrm[1] = read_bef32_fast(np + 0x4u);
                        nrm[2] = read_bef32_fast(np + 0x8u);
                        nrm[3] = read_bef32_fast(np + 0xCu);
                    } else {
                        nrm[0] = def_nrm[0]; nrm[1] = def_nrm[1];
                        nrm[2] = def_nrm[2]; nrm[3] = def_nrm[3];
                    }

                    float uv[2];
                    if (has_uv == 1u) {
                        const uint32_t uv_a = (vi < n_verts && uv_arr) ? (uv_arr + vi * 12u) : 0u;
                        uv[0] = read_bef32_fast(vm_base + uv_a + 0x0u);
                        uv[1] = read_bef32_fast(vm_base + uv_a + 0x4u);
                    } else {
                        uv[0] = def_uv[0];
                        uv[1] = def_uv[1];
                    }

                    cv_pack_vertex_attr_impl(out_p, desc_pos, [&pos](uint32_t i) { return pos[i]; });
                    cv_pack_vertex_attr_impl(out_p, desc_nrm, [&nrm](uint32_t i) { return nrm[i]; });
                    cv_pack_vertex_attr_impl(out_p, desc_uv,  [&uv ](uint32_t i) { return uv[i]; });
                    out_p += vtx_stride;
                }
            }
        }
    } else {
        /* Direct vertex list */
        const float nrm_scale = read_bef32_fast(vm_base + 0x0001D5CCu); /* (0x2<<16) - 10804 */
        for (uint32_t g = 0; g < num_groups; ++g) {
            const uint32_t grp_addr = groups_base + g * 0x10u;
            const uint32_t n_verts  = read_be32_fast(vm_base + grp_addr + 0x0u);
            const uint8_t  flip_nrm = vm_base[grp_addr + 0x4u];
            const uint8_t  has_uv   = vm_base[grp_addr + 0x5u];
            const uint32_t idx_arr  = read_be32_fast(vm_base + grp_addr + 0x8u);
            const uint32_t uv_arr   = read_be32_fast(vm_base + grp_addr + 0xCu);

            for (uint32_t vi = 0; vi < n_verts; ++vi) {
                const uint32_t vidx = idx_arr
                                    ? read_be32_fast(vm_base + idx_arr + vi * 4u)
                                    : read_be32_fast(vm_base);

                const uint8_t* pp = vm_base + ((pos_base + (vidx << 4)) & ~0xFu);
                const float pos[4] = {
                    read_bef32_fast(pp + 0x0u), read_bef32_fast(pp + 0x4u),
                    read_bef32_fast(pp + 0x8u), read_bef32_fast(pp + 0xCu)
                };

                float nrm[4];
                if (has_normals) {
                    const uint8_t* np = vm_base + ((nrm_base + (vidx << 4)) & ~0xFu);
                    nrm[0] = read_bef32_fast(np + 0x0u);
                    nrm[1] = read_bef32_fast(np + 0x4u);
                    nrm[2] = read_bef32_fast(np + 0x8u);
                    nrm[3] = read_bef32_fast(np + 0xCu);
                    if (flip_nrm != 0u) {
                        nrm[0] *= nrm_scale;
                        nrm[1] *= nrm_scale;
                        nrm[2] *= nrm_scale;
                        nrm[3] *= nrm_scale;
                    }
                } else {
                    nrm[0] = def_nrm[0]; nrm[1] = def_nrm[1];
                    nrm[2] = def_nrm[2]; nrm[3] = def_nrm[3];
                }

                float uv[2];
                if (has_uv == 1u) {
                    const uint32_t uv_a = uv_arr ? (uv_arr + vi * 12u) : 0u;
                    uv[0] = read_bef32_fast(vm_base + uv_a + 0x0u);
                    uv[1] = read_bef32_fast(vm_base + uv_a + 0x4u);
                } else {
                    uv[0] = def_uv[0];
                    uv[1] = def_uv[1];
                }

                cv_pack_vertex_attr_impl(out_p, desc_pos, [&pos](uint32_t i) { return pos[i]; });
                cv_pack_vertex_attr_impl(out_p, desc_nrm, [&nrm](uint32_t i) { return nrm[i]; });
                cv_pack_vertex_attr_impl(out_p, desc_uv,  [&uv ](uint32_t i) { return uv[i]; });
                out_p += vtx_stride;
            }
        }
    }
}

/* Hook 16: Recursive tree traversal (EU 0x00549F7C) -- 4.0% of guest CPU */
static uint32_t cv_tree_traverse_00549F7C(uint32_t tree_base, uint32_t node_val,
                                          uint32_t entries, uint32_t out_ptr)
{
    const uint32_t hi = node_val & 0xFFFFF000u;
    if (hi != 0u) {
        write_be32_fast(vm_base + out_ptr, hi);
        out_ptr += 4u;
    }

    const uint32_t node_addr = tree_base + ((node_val & 0x0FFFu) << 3);
    const uint32_t hdr       = read_be32_fast(vm_base + node_addr + 0x0u);
    uint32_t       child_ptr = read_be32_fast(vm_base + node_addr + 0x4u);
    const uint32_t n_branch  = (hdr >> 6) & 0x3Fu;
    const uint32_t n_leaf    = hdr & 0x3Fu;

    for (uint32_t i = 0; i < n_branch; ++i) {
        const uint32_t idx  = read_be16_fast(vm_base + child_ptr);
        child_ptr += 2u;
        const uint32_t cval = read_be32_fast(vm_base + entries + (idx << 3) + 4u);
        out_ptr = cv_tree_traverse_00549F7C(tree_base, cval, entries, out_ptr);
    }

    for (uint32_t i = 0; i < n_leaf; ++i) {
        const uint32_t idx  = read_be16_fast(vm_base + child_ptr);
        child_ptr += 2u;
        const uint32_t lval = read_be32_fast(vm_base + entries + (idx << 3) + 4u);
        if (lval != 0u) {
            write_be32_fast(vm_base + out_ptr, lval);
            out_ptr += 4u;
        }
    }

    return out_ptr;
}

void func_00549F7C(ppu_context* ctx)
{
    if (!native_hot_enabled() || !vm_base) {
        cv_original_00549F7C(ctx);
        return;
    }

    const uint32_t obj       = (uint32_t)ctx->gpr[3];
    const uint32_t node_val  = (uint32_t)ctx->gpr[4];
    const uint32_t entries   = (uint32_t)ctx->gpr[5];
    const uint32_t out_ptr   = (uint32_t)ctx->gpr[6];
    const uint32_t tree_base = read_be32_fast(vm_base + obj + 0x48u);

    ctx->gpr[3] = cv_tree_traverse_00549F7C(tree_base, node_val, entries, out_ptr);
}

/* =========================================================================
 * Hook 15: Frustum Culling Telemetry + Safe Culling (EU 0x001D2028 njCnkDrawModel)
 * ========================================================================= */
static bool frustum_log_enabled()
{
    static const bool en = []() {
        const char* v = std::getenv("CV_FRUSTUM_LOG");
        return v && v[0] && std::strcmp(v, "0") != 0;
    }();
    return en;
}

static bool frustum_cull_enabled()
{
    static const bool en = []() {
        const char* v = std::getenv("CV_FRUSTUM_CULL");
        return v && v[0] && std::strcmp(v, "0") != 0;
    }();
    return en;
}

struct FrustumStats {
    uint64_t calls = 0;
    uint64_t game_culled = 0;
    uint64_t our_culled = 0;
    uint64_t flag_off = 0;
    uint64_t no_cull = 0;
    uint64_t bbox_on = 0;
    uint64_t r_zero = 0;
    uint64_t would_cull_15 = 0;
    uint64_t v32f0_calls = 0;
    uint64_t v32f0_offscreen = 0;
    uint64_t v32f0_verts = 0;
    uint64_t v32f0_offscreen_verts = 0;
    std::chrono::steady_clock::time_point last_log = std::chrono::steady_clock::now();
};
static FrustumStats g_fstats;

static void cv_frustum_record_32f0(uint32_t count, bool all_offscreen)
{
    g_fstats.v32f0_calls++;
    g_fstats.v32f0_verts += count;
    if (all_offscreen) {
        g_fstats.v32f0_offscreen++;
        g_fstats.v32f0_offscreen_verts += count;
    }
}

static bool sphere_outside_frustum_15(float cx, float cy, float cz, float r,
                                      float z_near, float z_far,
                                      float clip_x, float clip_y,
                                      float proj_d, float ax, float ay)
{
    if (!(r > 0.0f) || std::isnan(cx) || std::isnan(cy) || std::isnan(cz) || std::isnan(r))
        return false;
    const float rp = r * 1.15f;
    if (cz + rp < z_near) return true;
    if (z_far < cz - rp)  return true;
    if (std::fabs(cz) < 1e-4f) return false;
    const float inv_z = std::fabs(proj_d / cz);
    const float sx = cx * inv_z;
    const float sr = rp * inv_z;
    const float cx_lim = clip_x * 1.15f;
    if (sx + sr * ax < -cx_lim) return true;
    if (cx_lim < sx - sr * ax)  return true;
    const float sy = cy * inv_z;
    const float cy_lim = clip_y * 1.15f;
    if (sy + sr * ay < -cy_lim) return true;
    if (cy_lim < sy - sr * ay)  return true;
    return false;
}

void func_001D2028(ppu_context* ctx)
{
    const bool do_log  = frustum_log_enabled();
    const bool do_cull = frustum_cull_enabled();
    if ((!do_log && !do_cull) || !vm_base) {
        cv_original_001D2028(ctx);
        return;
    }

    const uint32_t model = (uint32_t)ctx->gpr[3];
    if (!model) {
        cv_original_001D2028(ctx);
        return;
    }

    const uint32_t cull_on = read_be32_fast(vm_base + 0x00AECE70u);
    const uint32_t no_cull = read_be32_fast(vm_base + 0x00AECE78u);
    const uint32_t bbox_on = read_be32_fast(vm_base + 0x00AECEA0u);
    const float r          = read_bef32_fast(vm_base + model + 0x14u);

    const float z_near = read_bef32_fast(vm_base + 0x00CB7350u);
    const float z_far  = read_bef32_fast(vm_base + 0x00CB7354u);
    const float clip_x = read_bef32_fast(vm_base + 0x00CB7358u);
    const float clip_y = read_bef32_fast(vm_base + 0x00CB735Cu);
    const float proj_d = read_bef32_fast(vm_base + 0x00ADCB10u);
    const float ax     = read_bef32_fast(vm_base + 0x00ABEDC0u);
    const float ay     = read_bef32_fast(vm_base + 0x00ABEDC4u);

    bool outside = false;
    if (no_cull == 0u && bbox_on == 0u && r > 0.0f) {
        const uint32_t mat_addr = read_be32_fast(vm_base + 0x00ABED90u);
        if (mat_addr) {
            const uint8_t* m = vm_base + mat_addr;
            const float x = read_bef32_fast(vm_base + model + 0x08u);
            const float y = read_bef32_fast(vm_base + model + 0x0Cu);
            const float z = read_bef32_fast(vm_base + model + 0x10u);
            const float cx = read_bef32_fast(m + 0x00) * x + read_bef32_fast(m + 0x10) * y + read_bef32_fast(m + 0x20) * z + read_bef32_fast(m + 0x30);
            const float cy = read_bef32_fast(m + 0x04) * x + read_bef32_fast(m + 0x14) * y + read_bef32_fast(m + 0x24) * z + read_bef32_fast(m + 0x34);
            const float cz = read_bef32_fast(m + 0x08) * x + read_bef32_fast(m + 0x18) * y + read_bef32_fast(m + 0x28) * z + read_bef32_fast(m + 0x38);
            outside = sphere_outside_frustum_15(cx, cy, cz, r, z_near, z_far, clip_x, clip_y, proj_d, ax, ay);
        }
    }

    if (do_cull && outside) {
        write_be32_fast(vm_base + 0x00ABEDF8u, model);
        ctx->gpr[3] = 0;
        if (do_log) {
            g_fstats.calls++;
            g_fstats.our_culled++;
        }
        return;
    }

    cv_original_001D2028(ctx);

    if (do_log) {
        g_fstats.calls++;
        if (ctx->gpr[3] == 0) g_fstats.game_culled++;
        if (cull_on == 0u)    g_fstats.flag_off++;
        if (no_cull != 0u)    g_fstats.no_cull++;
        if (bbox_on != 0u)    g_fstats.bbox_on++;
        if (!(r > 0.0f))      g_fstats.r_zero++;
        if (outside && ctx->gpr[3] != 0) g_fstats.would_cull_15++;

        if ((g_fstats.calls & 0xFFu) == 0u) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_fstats.last_log).count();
            if (ms >= 2000) {
                std::fprintf(stderr,
                    "[FRUSTUM] 1D2028 calls=%llu game_cull=%llu our_cull=%llu would_cull15=%llu "
                    "flag_off=%llu no_cull=%llu bbox=%llu r0=%llu | "
                    "32F0 calls=%llu off=%llu verts=%llu off_v=%llu | "
                    "zn=%.2f zf=%.1f cx=%.1f cy=%.1f pd=%.1f ax=%.2f ay=%.2f\n",
                    (unsigned long long)g_fstats.calls,
                    (unsigned long long)g_fstats.game_culled,
                    (unsigned long long)g_fstats.our_culled,
                    (unsigned long long)g_fstats.would_cull_15,
                    (unsigned long long)g_fstats.flag_off,
                    (unsigned long long)g_fstats.no_cull,
                    (unsigned long long)g_fstats.bbox_on,
                    (unsigned long long)g_fstats.r_zero,
                    (unsigned long long)g_fstats.v32f0_calls,
                    (unsigned long long)g_fstats.v32f0_offscreen,
                    (unsigned long long)g_fstats.v32f0_verts,
                    (unsigned long long)g_fstats.v32f0_offscreen_verts,
                    z_near, z_far, clip_x, clip_y, proj_d, ax, ay);
                std::fflush(stderr);
                g_fstats = FrustumStats{};
                g_fstats.last_log = now;
            }
        }
    }
}

#endif /* EU */
