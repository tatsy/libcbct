# LibCBCT

[![Build](https://github.com/tatsy/libcbct/actions/workflows/build.yaml/badge.svg)](https://github.com/tatsy/libcbct/actions/workflows/build.yaml)
[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

## Get started

### Build

```shell
git clone https://github.com/tatsy/libcbct.git --depth 1 --submodules
cd libcbct
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### Run

```shell
./cbct_exe -c /path/to/data/config.json
```

### File structure

```text
/path/to/data/ +- config.json         # Configuration file
               +- projections/        # Folder containing projection images
                 +- proj_0000.tif     # Projection image at angle 0
                 +- proj_0001.tif     # Projection image at angle 1
                 +- ...
               +- output/             # Folder to save reconstructed volume
```

### Example configuration file (JSON format)

```json
{
  "SOD": 99.19738,
  "SDD": 449.75,
  "PixelSizeX": 0.178,
  "PixelSizeY": 0.178,
  "DetWidth": 896,
  "DetHeight": 896,
  "NumberOfProj": 1000,
  "Clockwise": true
}
```

## License

[CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/), 2023-2025 (c) Tatsuya Yatagawa
