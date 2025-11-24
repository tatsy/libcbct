import argparse
import os
from pathlib import Path

import numpy as np
import tifffile


def tofile(input_path: str | Path, out_path: str | Path, width: int, height: int, x_shift: int = 0):
    input_path = Path(input_path)
    out_path = Path(out_path)

    raw = np.fromfile(input_path, dtype=np.uint16)
    raw = raw.reshape((height, width))
    raw = np.flip(raw, axis=0)

    brt_path = input_path.parent / 'data.brt'
    if brt_path.exists():
        brt = np.fromfile(brt_path, dtype=np.uint16)
        brt = brt.reshape((height, width))
    else:
        brt = None

    drk_path = input_path.parent / 'data.drk'
    if drk_path.exists():
        drk = np.fromfile(drk_path, dtype=np.uint16)
        drk = drk.reshape((height, width))
    else:
        drk = None

    if brt is not None and drk is not None:
        raw = (raw - drk) / (brt - drk) * 50000
        raw = np.clip(raw, 0, 50000).astype(np.uint16)

    if x_shift > 0:
        raw = np.pad(raw, ((0, 0), (x_shift, 0)), mode='edge')[:, :-x_shift]
    elif x_shift < 0:
        raw = np.pad(raw, ((0, 0), (0, -x_shift)), mode='edge')[:, -x_shift:]

    out_path.parent.mkdir(parents=True, exist_ok=True)
    tifffile.imwrite(
        out_path,
        raw,
        compression='zlib',
        compressionargs={'level': 3},
    )


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', required=True, help='Input raw file path')
    parser.add_argument('-o', '--output', type=str, default='tif')
    parser.add_argument(
        '--size',
        nargs=2,
        type=int,
        required=True,
        help='Size of the image (width height)',
    )
    parser.add_argument('--x_shift', type=int, default=0, help='Shift in x direction')
    args = parser.parse_args()

    input_path = Path(args.input)
    width, height = args.size

    out_dir = input_path / args.output / 'projections'
    out_dir.mkdir(parents=True, exist_ok=True)

    if os.path.isfile(args.input):
        out_path = out_dir / (input_path.stem + '.tif')
        tofile(str(input_path), str(out_path), width, height, args.x_shift)
    elif input_path.is_dir():
        files = sorted(list(input_path.glob('*.raw')))
        for i, file in enumerate(files):
            out_path = out_dir / (file.stem + '.tif')
            tofile(str(file), str(out_path), width, height, args.x_shift)
            print(f'({i + 1}/{len(files)}) Converting {file} to {out_path}')
