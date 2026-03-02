#!/usr/bin/env python3
import argparse
import glob
from pathlib import Path

import numpy as np


def list_step_files(tmp_tdm_dir: Path):
    files = sorted([Path(p) for p in glob.glob(str(tmp_tdm_dir / "*")) if Path(p).name.isdigit()])
    if not files:
        raise RuntimeError(f"No numeric step files found in {tmp_tdm_dir}")
    return files


def read_step(path: Path):
    arr = np.fromfile(path, dtype=np.complex128)
    if arr.size % 3 != 0:
        raise RuntimeError(f"{path} length {arr.size} is not divisible by 3")
    return arr


def overlap_cos(a: np.ndarray, b: np.ndarray):
    den = np.linalg.norm(a) * np.linalg.norm(b)
    if den < 1e-30:
        return 0.0
    return float(abs(np.vdot(a, b)) / den)


def main():
    ap = argparse.ArgumentParser(
        description="Rephase tmpTDM files and write corrected copies to another directory."
    )
    ap.add_argument("--input-dir", required=True, help="Original tmpTDM directory")
    ap.add_argument("--output-dir", required=True, help="Output directory for corrected copies")
    ap.add_argument(
        "--skip-c0123",
        action="store_true",
        help="Do not generate corrected c0123 in output directory",
    )
    args = ap.parse_args()

    in_dir = Path(args.input_dir).resolve()
    out_dir = Path(args.output_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    step_files = list_step_files(in_dir)
    nsteps = len(step_files)
    print(f"Found {nsteps} step files: {step_files[0].name} -> {step_files[-1].name}")

    # Compute baseline continuity and copy step-1 unchanged.
    pre = []
    prev_raw = read_step(step_files[0])
    prev_raw.tofile(out_dir / step_files[0].name)
    abs_phase = np.ones(nsteps, dtype=np.complex128)

    # Rephase subsequent steps against corrected previous step.
    prev_corr = prev_raw.copy()
    for i in range(1, nsteps):
        cur_raw = read_step(step_files[i])
        pre.append(overlap_cos(prev_raw, cur_raw))

        ov = np.vdot(prev_corr, cur_raw)
        ph = np.exp(-1j * np.angle(ov)) if abs(ov) > 1e-14 else (1.0 + 0.0j)
        abs_phase[i] = abs_phase[i - 1] * ph
        cur_corr = cur_raw * ph
        cur_corr.tofile(out_dir / step_files[i].name)

        prev_raw = cur_raw
        prev_corr = cur_corr

    # Continuity after correction (using output files)
    post = []
    prev = read_step(out_dir / step_files[0].name)
    for i in range(1, nsteps):
        cur = read_step(out_dir / step_files[i].name)
        post.append(overlap_cos(prev, cur))
        prev = cur

    print(
        "Adjacent overlap cosine (mean/min): "
        f"before {np.mean(pre):.6f}/{np.min(pre):.6f}, "
        f"after {np.mean(post):.6f}/{np.min(post):.6f}"
    )

    if not args.skip_c0123:
        in_c0123 = in_dir / "c0123"
        if in_c0123.exists():
            raw = np.fromfile(in_c0123, dtype=np.complex128)
            nrec = nsteps - 1
            if raw.size % nrec != 0:
                raise RuntimeError(
                    f"{in_c0123} size {raw.size} cannot be split into {nrec} records"
                )
            rec_len = raw.size // nrec
            arr = raw.reshape(nrec, rec_len)
            # Record r corresponds to interval starting at step (r+1).
            for r in range(nrec):
                arr[r] *= abs_phase[r]
            arr.ravel().tofile(out_dir / "c0123")
            print(f"Wrote corrected c0123 to {out_dir / 'c0123'}")
        else:
            print("Input c0123 not found; skipped.")

    print(f"Done. Corrected files are in: {out_dir}")


if __name__ == "__main__":
    main()

