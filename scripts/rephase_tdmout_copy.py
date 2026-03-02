#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

import numpy as np


PAIR_RE = re.compile(r"\(([+-]?\d+\.\d+E[+-]\d+),([+-]?\d+\.\d+E[+-]\d+)\)")
NVNC_RE = re.compile(r"#\s*Nv\s*=\s*(\d+)\s*,\s*Nc\s*=\s*(\d+)")


def parse_tdmout(path: Path):
    lines = path.read_text().splitlines()
    nv = nc = None
    preamble = []
    block_headers = []
    data_rows = []
    in_blocks = False

    for ln in lines:
        m = NVNC_RE.search(ln)
        if m:
            nv, nc = int(m.group(1)), int(m.group(2))

        if ln.startswith("#"):
            if "kpoint" in ln:
                block_headers.append(ln)
                in_blocks = True
            elif not in_blocks:
                preamble.append(ln)
            continue

        if not ln.strip():
            continue

        pairs = PAIR_RE.findall(ln)
        if not pairs:
            continue
        data_rows.append([complex(float(r), float(i)) for r, i in pairs])

    if nv is None or nc is None:
        raise RuntimeError(f"Cannot parse Nv/Nc from {path}")
    if not block_headers:
        raise RuntimeError(f"Cannot parse kpoint headers from {path}")

    rows_per_block = 3 * nv
    if len(data_rows) != len(block_headers) * rows_per_block:
        raise RuntimeError(
            f"{path}: row mismatch, got {len(data_rows)} rows but expect {len(block_headers) * rows_per_block}"
        )
    for row in data_rows:
        if len(row) != nc:
            raise RuntimeError(f"{path}: row has {len(row)} entries, expected Nc={nc}")

    nblocks = len(block_headers)
    ntrans = nblocks * nv * nc
    arr = np.zeros((ntrans, 3), dtype=np.complex128)

    for ib in range(nblocks):
        base = ib * rows_per_block
        for idir in range(3):
            for iv in range(nv):
                row = data_rows[base + idir * nv + iv]
                for ic in range(nc):
                    tid = ib * (nv * nc) + iv + ic * nv
                    arr[tid, idir] = row[ic]

    return preamble, block_headers, nv, nc, arr


def write_tdmout(path: Path, preamble, block_headers, nv, nc, arr):
    path.parent.mkdir(parents=True, exist_ok=True)
    nblocks = len(block_headers)
    with path.open("w") as f:
        for ln in preamble:
            f.write(ln + "\n")
        if preamble and preamble[-1].strip():
            f.write("\n")

        for ib in range(nblocks):
            f.write(block_headers[ib] + "\n")
            for idir in range(3):
                for iv in range(nv):
                    out = []
                    for ic in range(nc):
                        tid = ib * (nv * nc) + iv + ic * nv
                        z = arr[tid, idir]
                        out.append(f"({z.real:+.12E},{z.imag:+.12E})")
                    f.write("".join(out) + "\n")
                if idir < 2:
                    f.write("\n")
            if ib < nblocks - 1:
                f.write("\n")


def overlap_cos(a: np.ndarray, b: np.ndarray):
    den = np.linalg.norm(a) * np.linalg.norm(b)
    if den < 1e-30:
        return 0.0
    return float(abs(np.vdot(a, b)) / den)


def main():
    ap = argparse.ArgumentParser(description="Write rephased copies of run/*/tdmout into a separate directory.")
    ap.add_argument("--run-dir", required=True, help="Path to run directory containing numeric step subdirectories")
    ap.add_argument("--out-dir", required=True, help="Output directory for corrected tdmout files")
    args = ap.parse_args()

    run_dir = Path(args.run_dir).resolve()
    out_dir = Path(args.out_dir).resolve()
    step_dirs = sorted([p for p in run_dir.iterdir() if p.is_dir() and p.name.isdigit()])
    step_dirs = [p for p in step_dirs if (p / "tdmout").exists()]
    if not step_dirs:
        raise RuntimeError(f"No step directories with tdmout found under {run_dir}")

    print(f"Found {len(step_dirs)} tdmout files: {step_dirs[0].name} -> {step_dirs[-1].name}")

    parsed = []
    for d in step_dirs:
        parsed.append((d.name, *parse_tdmout(d / "tdmout")))

    pre = []
    post = []
    prev_raw = parsed[0][-1]
    prev_corr = prev_raw.copy()

    # Write first file unchanged.
    name0, preamble0, hdr0, nv0, nc0, arr0 = parsed[0]
    write_tdmout(out_dir / name0 / "tdmout", preamble0, hdr0, nv0, nc0, arr0)

    for i in range(1, len(parsed)):
        name, preamble, hdrs, nv, nc, raw = parsed[i]
        pre.append(overlap_cos(prev_raw.ravel(), raw.ravel()))

        corr = raw.copy()
        for tid in range(corr.shape[0]):
            ov = np.vdot(prev_corr[tid], corr[tid])
            if abs(ov) > 1e-14:
                corr[tid] *= np.exp(-1j * np.angle(ov))

        write_tdmout(out_dir / name / "tdmout", preamble, hdrs, nv, nc, corr)
        post.append(overlap_cos(prev_corr.ravel(), corr.ravel()))

        prev_raw = raw
        prev_corr = corr

    print(
        "Adjacent overlap cosine (mean/min): "
        f"before {np.mean(pre):.6f}/{np.min(pre):.6f}, "
        f"after {np.mean(post):.6f}/{np.min(post):.6f}"
    )
    print(f"Done. Corrected copies written to: {out_dir}")


if __name__ == "__main__":
    main()

