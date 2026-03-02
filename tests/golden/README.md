# Golden Reference Images

This directory contains reference PNG images used by the shader visual regression tests.

## What are golden images?

Golden images are pre-captured frames that represent the expected visual output of Goggles under specific configurations. The visual tests compare live-rendered output against these references pixel-by-pixel within a configurable tolerance.

## Tracked files

- `shader_bypass_quadrant.png` — output with no shader (passthrough mode), quadrant client input
- `shader_zfast_quadrant.png` — output with the zfast-crt shader applied, quadrant client input

## How to generate golden images

Run the update task after a successful build:

```
pixi run update-golden
```

This rebuilds the project (debug preset by default) and captures fresh reference frames for both shader configurations. Override the preset with the `BUILD_PRESET` environment variable:

```
BUILD_PRESET=release pixi run update-golden
```

## How to review golden images

Inspect the PNG files directly:

```
feh tests/golden/shader_bypass_quadrant.png
feh tests/golden/shader_zfast_quadrant.png
```

The bypass image should show the raw quadrant client output. The zfast image should show the same content with CRT scanline and curvature effects applied.

## Git LFS

The PNG files are tracked with Git LFS. After cloning, run:

```
git lfs pull
```

to download the actual image data. Without this step the files will contain LFS pointer text rather than image data.

## When to update

Regenerate golden images after any intentional change to:

- Shader code (`shaders/retroarch/crt/crt-lottes-fast.slangp` or dependencies)
- Render pipeline output (color space, blending, format)
- Headless capture logic

Do not update goldens to paper over unexpected regressions — investigate the cause first.
