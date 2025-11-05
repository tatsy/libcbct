import argparse
import os
from pathlib import Path

import numpy as np
import tifffile
from tqdm import tqdm


def tofile(input_path, width, height):
    raw = np.fromfile(input_path, dtype=np.uint16)
    raw = raw.reshape((height, width))
    tif_name = input_path.replace(".raw", ".tif")
    tifffile.imwrite(tif_name, raw)
    os.remove(input_path)
    print(f"Converting {input_path} to {tif_name}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", required=True, help="Input raw file path")
    parser.add_argument(
        "--size",
        nargs=2,
        type=int,
        required=True,
        help="Size of the image (width height)",
    )
    args = parser.parse_args()
    width, height = args.size

    if os.path.isfile(args.input):
        tofile(args.input, width, height)
    elif os.path.isdir(args.input):
        files = sorted(list(Path(args.input).rglob("*.raw")))
        for file in tqdm(files, desc="Converting raw to tif"):
            tofile(str(file), width, height)
