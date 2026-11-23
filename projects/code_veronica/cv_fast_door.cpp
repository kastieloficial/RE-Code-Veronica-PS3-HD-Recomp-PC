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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

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
void func_000124F4(ppu_context* ctx);        /* scratch buffer allocator */
void func_007858F4(ppu_context* ctx);        /* MtFile::seek(obj, off, origin) */
void CV_FN_STOP_BANK(ppu_context* ctx);      /* (sound_mgr, bank_id) */

void func_001B2A48(ppu_context* ctx);

extern "C" uint8_t* vm_base;
extern "C" PPU_THREAD_LOCAL ppu_context* g_active_ctx;
extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);
extern "C" volatile unsigned char* ps3_patch_flag(const char* name);

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

static inline uint32_t read_be32_fast(const uint8_t* p)
{
    uint32_t v;
    std::memcpy(&v, p, 4);
#ifdef _MSC_VER
    return _byteswap_ulong(v);
#else
    return __builtin_bswap32(v);
#endif
}

static inline float read_bef32_fast(const uint8_t* p)
{
    uint32_t u = read_be32_fast(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
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

    for (int pass = 0; pass < 16; ++pass) {
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

    uint32_t pos_buf = 0, col_buf = 0, norm_buf = 0;
    {
        GuestCall alloc(ctx);
        pos_buf = alloc.run_u32(func_000124F4, 0u, count << 4);
        vm_write32(ctx_obj + 0x04, pos_buf);
        col_buf = alloc.run_u32(func_000124F4, 0u, count << 2);
        vm_write32(ctx_obj + 0x08, col_buf);
        vm_write32(ctx_obj + 0x10, count);
        if (count != 0) {
            norm_buf = alloc.run_u32(func_000124F4, 0u, count << 4);
            vm_write32(ctx_obj + 0x0C, norm_buf);
            vm_write32(ctx_obj + 0x14, count);
        }
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

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* v = vsrc + i * 0x20u;
            const float x = read_bef32_fast(v + 0x00);
            const float y = read_bef32_fast(v + 0x04);
            const float z = read_bef32_fast(v + 0x08);

            const float px = m00 * x + m10 * y + m20 * z + m30;
            const float py = m01 * x + m11 * y + m21 * z + m31;
            const float pz = m02 * x + m12 * y + m22 * z + m32;

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

#endif /* EU */
