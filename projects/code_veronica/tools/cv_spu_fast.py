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
    stats = {"clusters": 0, "clustered_funcs": 0, "goto_jumps": 0, "musttail_jumps": 0}
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
                lines.append(f"void {full_name}(spu_context* ctx) {{ {cluster_fn}(ctx, 0x{a}u); }}")
            lines.append("\n")
            out_parts.append("\n".join(lines))
        else:
            for a in comp:
                full_name, inner = funcs[a]

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
