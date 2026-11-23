"""Optimize lifted SPU C code by eliminating direct trampoline returns.

1. Merges weakly connected components (2 <= size <= 80) into single super-functions
   connected by `goto`, allowing the compiler to keep `ctx->gpr[...]` in XMM/YMM
   registers across basic blocks.
2. Replaces all remaining direct same-image trampoline returns with
   `__attribute__((musttail)) return target(ctx);`, eliminating `spu_drain_call`
   overhead and TLS trampoline dispatch on every branch.
"""
import argparse
import re
from collections import defaultdict
from pathlib import Path


NATIVE_SPU_0009_HELPERS = r"""
/* Native x64 C replacements for the hottest functions in spu_0009 (spu_execute2_task.elf). */
static inline uint32_t cv_spu_ls_rd32(const spu_context* ctx, uint32_t a) {
    uint32_t v;
    memcpy(&v, ctx->ls + (a & SPU_LS_MASK), 4);
    return SPU_BSWAP32(v);
}

static inline void cv_spu_ls_wr32(spu_context* ctx, uint32_t a, uint32_t v) {
    v = SPU_BSWAP32(v);
    memcpy(ctx->ls + (a & SPU_LS_MASK), &v, 4);
}

static inline void cv_spu_dma_loop(spu_context* ctx, uint32_t lsa, uint64_t ea,
                                   uint32_t size, uint32_t tag, uint32_t cmd) {
    while (size != 0u) {
        uint32_t align = (lsa | (uint32_t)ea) & 15u;
        uint32_t chunk;
        if (align == 0u && size >= 16u) {
            chunk = (size > 16384u) ? 16384u : (size & ~15u);
        } else if ((align & 7u) == 0u && size >= 8u) {
            chunk = 8u;
        } else if ((align & 3u) == 0u && size >= 4u) {
            chunk = 4u;
        } else if ((align & 1u) == 0u && size >= 2u) {
            chunk = 2u;
        } else {
            chunk = 1u;
        }
        u128 v_lsa = { ._u32 = { lsa, 0, 0, 0 } };
        u128 v_eah = { ._u32 = { (uint32_t)(ea >> 32), 0, 0, 0 } };
        u128 v_eal = { ._u32 = { (uint32_t)ea, 0, 0, 0 } };
        u128 v_sz  = { ._u32 = { chunk, 0, 0, 0 } };
        u128 v_tag = { ._u32 = { tag, 0, 0, 0 } };
        u128 v_cmd = { ._u32 = { cmd, 0, 0, 0 } };
        spu_wrch(ctx, MFC_LSA,   v_lsa);
        spu_wrch(ctx, MFC_EAH,   v_eah);
        spu_wrch(ctx, MFC_EAL,   v_eal);
        spu_wrch(ctx, MFC_Size,  v_sz);
        spu_wrch(ctx, MFC_TagID, v_tag);
        spu_wrch(ctx, MFC_Cmd,   v_cmd);
        lsa  += chunk;
        ea   += chunk;
        size -= chunk;
    }
}

static inline void cv_spu_dma_get_wait(spu_context* ctx, uint32_t lsa, uint64_t ea,
                                       uint32_t size, uint32_t tag) {
    cv_spu_dma_loop(ctx, lsa, ea, size, tag, 64u);
    u128 v_mask = { ._u32 = { 1u << tag, 0, 0, 0 } };
    u128 v_upd  = { ._u32 = { 2u, 0, 0, 0 } };
    spu_wrch(ctx, MFC_WrTagMask,   v_mask);
    spu_wrch(ctx, MFC_WrTagUpdate, v_upd);
    (void)spu_rdch(ctx, MFC_RdTagStat);
}

static void cv_native_spu_0009_00020D38(spu_context* ctx) {
    uint32_t lsa  = ctx->gpr[3]._u32[0];
    uint64_t ea   = ((uint64_t)ctx->gpr[4]._u32[0] << 32) | (uint64_t)ctx->gpr[4]._u32[1];
    uint32_t size = ctx->gpr[5]._u32[0];
    uint32_t tag  = ctx->gpr[6]._u32[0];
    uint32_t cmd  = ctx->gpr[7]._u32[0];
    cv_spu_dma_loop(ctx, lsa, ea, size, tag, cmd);
    SPU_RET(ctx);
}

static void cv_native_spu_0009_0000B630(spu_context* ctx) {
    const uint32_t state_ls    = ctx->gpr[3]._u32[0];
    const uint32_t base_vertex = ctx->gpr[4]._u32[0];

    /* 1. Cache vertex attribute table (320 bytes) in Local Store */
    const uint32_t ea_attr = cv_spu_ls_rd32(ctx, state_ls + 44u);
    if (ea_attr != cv_spu_ls_rd32(ctx, 0x22E10u)) {
        const uint32_t ls_dst = (ea_attr & 15u) + cv_spu_ls_rd32(ctx, 0x22E30u);
        cv_spu_ls_wr32(ctx, 0x22E20u, ls_dst);
        cv_spu_dma_get_wait(ctx, ls_dst, (uint64_t)ea_attr, 320u, 6u);
        cv_spu_ls_wr32(ctx, 0x22E10u, ea_attr);
    }
    uint32_t ls_attr = cv_spu_ls_rd32(ctx, 0x22E20u);
    cv_spu_ls_wr32(ctx, state_ls + 44u, ls_attr);

    /* 2. Cache vertex buffer pointer table (80 bytes) in Local Store */
    const uint32_t ea_vbuf = cv_spu_ls_rd32(ctx, state_ls + 20u);
    if (ea_vbuf != cv_spu_ls_rd32(ctx, 0x22E70u)) {
        const uint32_t ls_dst = (ea_vbuf & 15u) + cv_spu_ls_rd32(ctx, 0x22E90u);
        cv_spu_ls_wr32(ctx, 0x22E80u, ls_dst);
        cv_spu_dma_get_wait(ctx, ls_dst, (uint64_t)ea_vbuf, 80u, 7u);
        cv_spu_ls_wr32(ctx, 0x22E70u, ea_vbuf);
    }
    const uint32_t ls_vbuf = cv_spu_ls_rd32(ctx, 0x22E80u);
    cv_spu_ls_wr32(ctx, state_ls + 20u, ls_vbuf);

    /* 3. Emit NV4097_SET_VERTEX_DATA_ARRAY_OFFSET for streams 0..3 */
    for (uint32_t stream = 0u; stream <= 3u; ++stream) {
        const uint32_t stream_off = stream << 2;
        const uint32_t stride     = cv_spu_ls_rd32(ctx, state_ls + stream_off + 68u);
        const uint32_t v_off      = cv_spu_ls_rd32(ctx, state_ls + stream_off + 84u);
        const uint32_t vb_ea      = cv_spu_ls_rd32(ctx, ls_vbuf + stream_off);

        for (uint32_t i = 0u; ; ++i) {
            const uint32_t entry = ls_attr + i * 20u;
            if (cv_spu_ls_rd32(ctx, entry + 12u) == 0u) break;
            if (cv_spu_ls_rd32(ctx, entry + 16u) != stream) continue;

            const uint32_t attr_reg = cv_spu_ls_rd32(ctx, entry + 0u) & 0xFFu;
            const uint32_t attr_off = cv_spu_ls_rd32(ctx, entry + 4u);

            uint32_t vb_loc = 0u;
            uint32_t vb_base_off = 0u;
            if (vb_ea != 0u) {
                if (cv_spu_ls_rd32(ctx, 0x22EA0u + stream_off) != vb_ea) {
                    const uint32_t ls_vb = (vb_ea & 15u) + cv_spu_ls_rd32(ctx, 0x22EC0u + stream_off);
                    cv_spu_ls_wr32(ctx, 0x22EB0u + stream_off, ls_vb);
                    cv_spu_dma_get_wait(ctx, ls_vb + 4u, (uint64_t)vb_ea + 4u, 40u, 8u);
                    cv_spu_ls_wr32(ctx, 0x22EA0u + stream_off, vb_ea);
                }
                const uint32_t ls_vb = cv_spu_ls_rd32(ctx, 0x22EB0u + stream_off);
                vb_loc      = cv_spu_ls_rd32(ctx, ls_vb + 28u);
                vb_base_off = cv_spu_ls_rd32(ctx, ls_vb + 40u);
            }

            const uint32_t rsx_offset = vb_base_off + base_vertex * stride + v_off + attr_off;
            const uint32_t cache_addr = state_ls + (i << 2) + 3624u;
            if (cv_spu_ls_rd32(ctx, cache_addr) == rsx_offset) continue;

            const uint32_t cmd = cv_spu_ls_rd32(ctx, state_ls + 4u);
            uint32_t cur = cv_spu_ls_rd32(ctx, cmd + 8u);
            const uint32_t end = cv_spu_ls_rd32(ctx, cmd + 4u);
            int emit = 1;
            if (cur + 8u > end) {
                const uint32_t flush_fn = cv_spu_ls_rd32(ctx, cmd + 12u);
                u128 saved_lr = ctx->gpr[0];
                ctx->gpr[3] = (u128){ ._u32 = { cmd, 0, 0, 0 } };
                ctx->gpr[4] = (u128){ ._u32 = { 2u, 0, 0, 0 } };
                ctx->gpr[0] = spu_link(0xBB90);
                {
                    int32_t _si = (int32_t)ctx->image_id;
                    ctx->pc = flush_fn;
                    ctx->host_depth++;
                    spu_indirect_branch(ctx);
                    spu_drain_call(ctx, 0xBB90);
                    ctx->host_depth--;
                    spu_img_restore(ctx, _si);
                }
                ctx->gpr[0] = saved_lr;
                ls_attr = cv_spu_ls_rd32(ctx, state_ls + 44u);
                if (ctx->gpr[3]._u32[0] != 0u) {
                    emit = 0;
                } else {
                    cur = cv_spu_ls_rd32(ctx, cmd + 8u);
                }
            }
            if (emit) {
                cv_spu_ls_wr32(ctx, cur + 0u, 0x00040000u | (5760u + (attr_reg << 2)));
                cv_spu_ls_wr32(ctx, cur + 4u, rsx_offset | (vb_loc << 31));
                cv_spu_ls_wr32(ctx, cmd + 8u, cur + 8u);
            }
            cv_spu_ls_wr32(ctx, cache_addr, rsx_offset);
        }
    }

    /* 4. Restore original main-memory EAs */
    cv_spu_ls_wr32(ctx, state_ls + 20u, ea_vbuf);
    cv_spu_ls_wr32(ctx, state_ls + 44u, ea_attr);
    SPU_RET(ctx);
}
"""

NATIVE_SPU_0009_OVERRIDES = {
    "0000B630": "cv_native_spu_0009_0000B630(ctx);",
    "00020D38": "cv_native_spu_0009_00020D38(ctx);",
    "00020DF0": "cv_native_spu_0009_00020D38(ctx);",
}


def optimize_spu_source(src: str, prefix: str, max_cluster: int = 80) -> tuple[str, dict]:
    if "ctx->int_enable" in src or "spu_iret" in src:
        raise RuntimeError(f"Refusing to bypass trampolines on interrupt-using image {prefix}")

    fn_re = re.compile(
        rf"^void ({re.escape(prefix)}_spu_func_([0-9A-F]+))\(spu_context\* ctx\) \{{",
        re.M,
    )
    reg_pos = src.find("typedef struct { uint32_t addr;")
    if reg_pos == -1:
        reg_pos = src.find("static const spu_func_entry")
    if reg_pos == -1:
        reg_marker = f"void {prefix}_spu_recomp_register("
        reg_pos = src.find(reg_marker)
    if reg_pos == -1:
        raise RuntimeError(f"Could not find function table or register fn for {prefix}")

    matches = list(fn_re.finditer(src, 0, reg_pos))
    if not matches:
        raise RuntimeError(f"No functions found for {prefix}")

    header = src[: matches[0].start()]
    footer = src[reg_pos:]

    funcs: dict[str, tuple[str, str]] = {}  # addr -> (full_name, inner_body)
    order: list[str] = []
    for i, m in enumerate(matches):
        start = m.start()
        end = matches[i + 1].start() if i + 1 < len(matches) else reg_pos
        chunk = src[start:end]
        open_brace = chunk.find("{")
        close_brace = chunk.rfind("}")
        inner = chunk[open_brace + 1 : close_brace]
        addr = m.group(2)
        funcs[addr] = (m.group(1), inner)
        order.append(addr)

    tramp_re = re.compile(
        rf"\{{ ctx->pc = 0x[0-9A-Fa-f]+; g_spu_trampoline_fn = {re.escape(prefix)}_spu_func_([0-9A-F]+); return; \}}"
    )

    edges: dict[str, set[str]] = defaultdict(set)
    rev: dict[str, set[str]] = defaultdict(set)
    for addr, (_, inner) in funcs.items():
        for tgt in tramp_re.findall(inner):
            if tgt in funcs:
                edges[addr].add(tgt)
                rev[tgt].add(addr)

    # Find weakly connected components
    visited: set[str] = set()
    wccs: list[list[str]] = []
    for addr in order:
        if addr in visited:
            continue
        comp: list[str] = []
        q = [addr]
        visited.add(addr)
        while q:
            u = q.pop()
            comp.append(u)
            for v in edges[u] | rev[u]:
                if v not in visited:
                    visited.add(v)
                    q.append(v)
        comp.sort(key=lambda a: int(a, 16))
        wccs.append(comp)

    out_parts: list[str] = [header]
    overrides = NATIVE_SPU_0009_OVERRIDES if prefix == "spu_0009_at_00940C00" else {}
    if overrides:
        out_parts.append(NATIVE_SPU_0009_HELPERS + "\n")
    stats = {"clusters": 0, "clustered_funcs": 0, "goto_jumps": 0, "musttail_jumps": 0, "native_overrides": 0}
    cluster_id = 0

    for comp in wccs:
        if 2 <= len(comp) <= max_cluster:
            comp_set = set(comp)
            cid = cluster_id
            cluster_id += 1
            stats["clusters"] += 1
            stats["clustered_funcs"] += len(comp)

            cluster_fn = f"{prefix}_cluster_{cid}"
            lines = [f"static void {cluster_fn}(spu_context* ctx, uint32_t _entry) {{"]
            lines.append("    switch (_entry) {")
            for a in comp:
                lines.append(f"        case 0x{a}u: goto bb_{a};")
            lines.append("        default: return;")
            lines.append("    }")

            for a in comp:
                _, inner = funcs[a]

                def repl_cluster(m: re.Match) -> str:
                    tgt = m.group(1)
                    if tgt in comp_set:
                        stats["goto_jumps"] += 1
                        return f"goto bb_{tgt};"
                    # Should not occur in a WCC, but safe fallback
                    return m.group(0)

                patched = tramp_re.sub(repl_cluster, inner)
                lines.append(f"bb_{a}: {{")
                lines.append(patched)
                lines.append("    return;")
                lines.append("}")

            lines.append("}\n")
            for a in comp:
                full_name, _ = funcs[a]
                if a in overrides:
                    stats["native_overrides"] += 1
                    lines.append(f"void {full_name}(spu_context* ctx) {{ {overrides[a]} }}")
                else:
                    lines.append(f"void {full_name}(spu_context* ctx) {{ {cluster_fn}(ctx, 0x{a}u); }}")
            lines.append("\n")
            out_parts.append("\n".join(lines))
        else:
            for a in comp:
                full_name, inner = funcs[a]
                if a in overrides:
                    stats["native_overrides"] += 1
                    out_parts.append(f"void {full_name}(spu_context* ctx) {{ {overrides[a]} }}\n\n")
                    continue

                def repl_tail(m: re.Match) -> str:
                    tgt = m.group(1)
                    stats["musttail_jumps"] += 1
                    return f"{{ __attribute__((musttail)) return {prefix}_spu_func_{tgt}(ctx); }}"

                patched = tramp_re.sub(repl_tail, inner)
                out_parts.append(f"void {full_name}(spu_context* ctx) {{{patched}}}\n\n")

    out_parts.append(footer)
    return "".join(out_parts), stats


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--prefix", required=True)
    p.add_argument("--max-cluster", type=int, default=80)
    args = p.parse_args()

    src = args.input.read_text(encoding="utf-8", errors="surrogateescape")
    out, stats = optimize_spu_source(src, args.prefix, args.max_cluster)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    hdr_in = args.input.with_name("spu_recomp.h")
    if hdr_in.exists():
        hdr_out = args.output.with_name("spu_recomp.h")
        hdr_txt = hdr_in.read_text(encoding="utf-8", errors="surrogateescape")
        if not hdr_out.exists() or hdr_out.read_text(encoding="utf-8", errors="surrogateescape") != hdr_txt:
            hdr_out.write_text(hdr_txt, encoding="utf-8", errors="surrogateescape", newline="\n")
    if not args.output.exists() or args.output.read_text(encoding="utf-8", errors="surrogateescape") != out:
        args.output.write_text(out, encoding="utf-8", errors="surrogateescape", newline="\n")
    print(f"[cv_spu_fast] {args.prefix}: {stats}")


if __name__ == "__main__":
    main()
