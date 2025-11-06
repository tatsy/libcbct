import argparse
import os
from pathlib import Path

import numpy as np
import tifffile


def tofile(input_path: str, output_path: str, width: int, height: int, x_shift: int = 0):
    raw = np.fromfile(input_path, dtype=np.uint16)
    raw = raw.reshape((height, width))

    if x_shift > 0:
        raw = np.pad(raw, ((0, 0), (x_shift, 0)), mode='edge')[:, :-x_shift]
    elif x_shift < 0:
        raw = np.pad(raw, ((0, 0), (0, -x_shift)), mode='edge')[:, -x_shift:]

    Path(output_path).parent.mkdir(parents=True, exist_ok=True)
    tifffile.imwrite(output_path, raw)


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

    if os.path.isfile(args.input):
        out_path = input_path.parent / args.output / (input_path.stem + '.tif')
        tofile(str(input_path), str(out_path), width, height, args.x_shift)
    elif input_path.is_dir():
        files = sorted(list(input_path.glob('*.raw')))
        for i, file in enumerate(files):
            out_path = input_path / args.output / (file.stem + '.tif')
            tofile(str(file), str(out_path), width, height, args.x_shift)
            print(f'({i + 1}/{len(files)}) Converting {file} to {out_path}')
