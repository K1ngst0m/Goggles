# Shader Compatibility Report

RetroArch shader preset compatibility with Goggles filter chain.

> **Note:** This report only covers **compilation status** (parse + preprocess).
> Passing compilation does not guarantee visual correctness.

**Tested versions:** Goggles `d7475a9` / slang-shaders `e24402e`

## Overview

**Total:** 1153/1906 presets compile (60%)

## By Category

| Category | Pass Rate | Status |
|----------|-----------|--------|
| anamorphic | 1/1 | ✅ |
| anti-aliasing | 9/9 | ✅ |
| bezel | 239/958 | ⚠️ |
| blurs | 17/17 | ✅ |
| border | 43/43 | ✅ |
| cel | 3/3 | ✅ |
| crt | 109/117 | ⚠️ |
| deblur | 2/2 | ✅ |
| deinterlacing | 11/11 | ✅ |
| denoisers | 7/7 | ✅ |
| dithering | 17/17 | ✅ |
| downsample | 59/59 | ✅ |
| edge-smoothing | 95/95 | ✅ |
| film | 2/2 | ✅ |
| gpu | 3/3 | ✅ |
| handheld | 67/67 | ✅ |
| hdr | 4/29 | ⚠️ |
| interpolation | 42/42 | ✅ |
| linear | 1/1 | ✅ |
| misc | 36/36 | ✅ |
| motionblur | 8/8 | ✅ |
| motion-interpolation | 1/1 | ✅ |
| nes_raw_palette | 5/5 | ✅ |
| ntsc | 25/25 | ✅ |
| pal | 3/3 | ✅ |
| pixel-art-scaling | 22/23 | ⚠️ |
| presets | 231/231 | ✅ |
| reshade | 55/55 | ✅ |
| scanlines | 8/8 | ✅ |
| sharpen | 6/6 | ✅ |
| stereoscopic-3d | 8/8 | ✅ |
| subframe-bfi | 6/6 | ✅ |
| vhs | 7/7 | ✅ |
| warp | 1/1 | ✅ |

## Details

<details>
<summary><strong>anamorphic</strong> (1/1)</summary>

| Preset | Status |
|--------|--------|
| `anamorphic.slangp` | ✅ |

</details>

<details>
<summary><strong>anti-aliasing</strong> (9/9)</summary>

| Preset | Status |
|--------|--------|
| `aa-shader-4.0-level2.slangp` | ✅ |
| `aa-shader-4.0.slangp` | ✅ |
| `advanced-aa.slangp` | ✅ |
| `fxaa+linear.slangp` | ✅ |
| `fxaa.slangp` | ✅ |
| `reverse-aa.slangp` | ✅ |
| `smaa+linear.slangp` | ✅ |
| `smaa+sharpen.slangp` | ✅ |
| `smaa.slangp` | ✅ |

</details>

<details>
<summary><strong>bezel</strong> (239/958)</summary>

| Preset | Status |
|--------|--------|
| `koko-aio/koko-aio-ng-DMG.slangp` | ✅ |
| `koko-aio/koko-aio-ng.slangp` | ✅ |
| `koko-aio/Presets-4.1/clean-scanlines-classic_take.slangp` | ❌ |
| `koko-aio/Presets-4.1/FXAA-bloom-immersive.slangp` | ❌ |
| `koko-aio/Presets-4.1/FXAA-bloom.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-BASE.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-bloom.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-Commodore_1084S-Night.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-Commodore_1084S-Night-wider.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-Commodore_1084S.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-Commodore_1084S-wider.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom-bezel-backimage.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom-bezelwider-classic_take.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom-ShinyBezel.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask-bloom.slangp` | ❌ |
| `koko-aio/Presets-4.1/monitor-slotmask.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-aperturegrille-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-aperturegrille-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-aperturegrille-bloom.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-aperturegrille.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-BASE.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-flickering-2nd-take.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-flickering.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-NTSC-1-classic_take.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-NTSC-1-selective-classic_take.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-NTSC-1-selective.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-NTSC-1.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-NTSC-2.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-PAL-my-old.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-slotmask-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-slotmask-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-slotmask-bloom.slangp` | ❌ |
| `koko-aio/Presets-4.1/tv-slotmask.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Dots_1-1.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Dots_4-3.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Dots-sharp_4-3.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyAdvance-Overlay-Night.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyAdvance-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyAdvance.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyColor-Overlay-IPS.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyColor-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyColor-Overlay-Taller-IPS.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyColor-Overlay-Taller.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyColor.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyMono-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyMono-Overlay-Taller.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyMono.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyPocket-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyPocket-Overlay-Taller.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameboyPocket.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameGear-Overlay-Night.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameGear-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/GameGear.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Generic-Handheld-RGB.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Lynx-Overlay-Night.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Lynx-Overlay.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/Lynx.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/PSP-Overlay-Night-Big.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/PSP-Overlay-Night-Big- Y_flip.slangp` | ✅ |
| `koko-aio/Presets_Handhelds-ng/PSP-Overlay-Night-Small.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/PSP-Overlay-Night-Small-Y_flip.slangp` | ❌ |
| `koko-aio/Presets_Handhelds-ng/PSP.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/koko-aio-ng.slangp` | ✅ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/clean-scanlines-classic_take.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/FXAA-bloom-immersive.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/FXAA-bloom.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-BASE.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-bloom.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-Commodore_1084S-Night.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-Commodore_1084S-Night-wider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-Commodore_1084S.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-Commodore_1084S-wider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom-bezel-backimage.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom-bezelwider-classic_take.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom-ShinyBezel.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask-bloom.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/monitor-slotmask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-aperturegrille-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-aperturegrille-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-aperturegrille-bloom.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-aperturegrille.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-BASE.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-flickering-2nd-take.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-flickering.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-NTSC-1-classic_take.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-NTSC-1-selective-classic_take.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-NTSC-1-selective.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-NTSC-1.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-NTSC-2.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-PAL-my-old.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-slotmask-bloom-bezel.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-slotmask-bloom-bezelwider.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-slotmask-bloom.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-4.1/tv-slotmask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets_Handhelds-ng/PSP-Overlay-Night-Big.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets_Handhelds-ng/PSP-Overlay-Night-Big- Y_flip.slangp` | ✅ |
| `koko-aio/Presets_HiresGames_Fast/Presets_Handhelds-ng/PSP-Overlay-Night-Small.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets_Handhelds-ng/PSP-Overlay-Night-Small-Y_flip.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets_Handhelds-ng/PSP.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Base.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Ambilight-immersive.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Balanced.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-crt-regale.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-for_1440pMin_HiNits.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-for_HigherNits.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-FXAA_sharp-aperturegrille.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-FXAA_sharp-Core_SlotMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-FXAA_sharp-Screen_SlotMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-New_slotmask_gm.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-New_slotmask_rgb.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Aperturegrille-Overmask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Aperturegrille.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Core_SlotMask-Overmask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Core_SlotMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask_Overlapped-oldpainless.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask-Chameleon.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller_Brighter.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller_Brightest.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-Screen_Hmask-ShadowMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-slotmask-TVL410.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/Monitor-slotmask-TVL500-for_1080p.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/TV/Tv-NTSC_Generic-AA_sharp-Selective.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/TV/Tv-NTSC_Megadrive-AA_sharp-Selective.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VectorGFX/Vector_neon_4_mame2003plus_defaults.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VectorGFX/Vector_std_4_mame2003plus_defaults.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VGA/Monitor-VGA-DoubleScan-Amber.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VGA/Monitor-VGA-DoubleScan-Green.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VGA/Monitor-VGA-DoubleScan-ShadowMask.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VGA/Monitor-VGA-DoubleScan.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/Presets-ng/VGA/Monitor-VGA-DoubleScan-XBR.slangp` | ❌ |
| `koko-aio/Presets_HiresGames_Fast/refs/bezel-dark.slangp` | ❌ |
| `koko-aio/Presets-ng/Base.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Ambilight-immersive.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Balanced.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-crt-regale.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-for_1440pMin_HiNits.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-for_HigherNits.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-FXAA_sharp-aperturegrille.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-FXAA_sharp-Core_SlotMask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-FXAA_sharp-Screen_SlotMask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-New_slotmask_gm.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-New_slotmask_rgb.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Aperturegrille-Overmask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Aperturegrille.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Core_SlotMask-Overmask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Core_SlotMask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask_Overlapped-oldpainless.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask-Chameleon.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller_Brighter.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller_Brightest.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-Screen_SlotMask_Taller.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-Screen_Hmask-ShadowMask.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-slotmask-TVL410.slangp` | ❌ |
| `koko-aio/Presets-ng/Monitor-slotmask-TVL500-for_1080p.slangp` | ❌ |
| `koko-aio/Presets-ng/TV/Tv-NTSC_Generic-AA_sharp-Selective.slangp` | ❌ |
| `koko-aio/Presets-ng/TV/Tv-NTSC_Megadrive-AA_sharp-Selective.slangp` | ❌ |
| `koko-aio/Presets-ng/VectorGFX/Vector_neon_4_mame2003plus_defaults.slangp` | ❌ |
| `koko-aio/Presets-ng/VectorGFX/Vector_std_4_mame2003plus_defaults.slangp` | ❌ |
| `koko-aio/Presets-ng/VGA/Monitor-VGA-DoubleScan-Amber.slangp` | ❌ |
| `koko-aio/Presets-ng/VGA/Monitor-VGA-DoubleScan-Green.slangp` | ❌ |
| `koko-aio/Presets-ng/VGA/Monitor-VGA-DoubleScan-ShadowMask.slangp` | ❌ |
| `koko-aio/Presets-ng/VGA/Monitor-VGA-DoubleScan.slangp` | ❌ |
| `koko-aio/Presets-ng/VGA/Monitor-VGA-DoubleScan-XBR.slangp` | ❌ |
| `koko-aio/refs/bezel-dark.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__0__SMOOTH-ADV__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__0__SMOOTH-ADV__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__0__SMOOTH-ADV__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__0__SMOOTH-ADV__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-PSP-960x544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-PSP_X-VIEWPORT_Y-272p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ-PSP_X-VIEWPORT_Y-544p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-320p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-320p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__LCD-GRID__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__1__ADV__LCD-GRID__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-3DS-1600x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-NDS-1280x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-PSP-960x544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-PSP_X-VIEWPORT_Y-272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-PSP_X-VIEWPORT_Y-544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__LCD-GRID__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__LCD-GRID__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__LCD-GRID__DREZ-PSP_X-VIEWPORT_Y-272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__PASSTHROUGH__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__PASSTHROUGH__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__3__STD__PASSTHROUGH__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-MINI__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-MINI__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-MINI-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-NTSC__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__PASSTHROUGH__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__PASSTHROUGH__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/MBZ__5__POTATO__PASSTHROUGH__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__0__SMOOTH-ADV__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__0__SMOOTH-ADV__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__0__SMOOTH-ADV__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__0__SMOOTH-ADV__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-PSP-960x544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-PSP_X-VIEWPORT_Y-272p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ-PSP_X-VIEWPORT_Y-544p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-320p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-320p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__LCD-GRID__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__1__ADV__LCD-GRID__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-3DS-1600x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-NDS-1280x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-PSP-960x544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-PSP_X-VIEWPORT_Y-272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-PSP_X-VIEWPORT_Y-544.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-224p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-NTSC__DREZ-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-NTSC__DREZ_X-VIEWPORT_Y-240p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__LCD-GRID__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__LCD-GRID__DREZ-PSP-480x272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__LCD-GRID__DREZ-PSP_X-VIEWPORT_Y-272.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__PASSTHROUGH__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__PASSTHROUGH__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__3__STD__PASSTHROUGH__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-MINI__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-MINI__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-MINI-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-MINI-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-NTSC__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-NTSC__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__GDV-NTSC__DREZ_X-VIEWPORT_Y-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__PASSTHROUGH__DREZ-1080p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__PASSTHROUGH__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets_DREZ/Root_Presets/MBZ__5__POTATO__PASSTHROUGH__DREZ-VIEWPORT.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV-RESHADE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__1__ADV-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS-RESHADE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-GLASS-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-NO-REFLECT-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY-NO-TUBE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__2__ADV-SCREEN-ONLY-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-GLASS-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__3__STD-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY-NO-TUBE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-SCREEN-ONLY-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/MBZ__5__POTATO-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__0__SMOOTH-ADV-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV-RESHADE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__1__ADV-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS-RESHADE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-GLASS-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-NO-REFLECT-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY-NO-TUBE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__2__ADV-SCREEN-ONLY-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-GLASS-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__3__STD-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-NO-REFLECT-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY-NO-TUBE-FX__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY-NO-TUBE-FX__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__4__STD-SCREEN-ONLY-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__EASYMODE.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__GDV-MINI-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__MEGATRON-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__MEGATRON.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO__PASSTHROUGH.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO-SUPER-XBR__GDV-NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Base_CRT_Presets/Root_Presets/MBZ__5__POTATO-SUPER-XBR__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Experimental/Guest-SlotMask-1.slangp` | ❌ |
| `Mega_Bezel/Presets/Experimental/Guest Slotmask 2023-02.slangp` | ✅ |
| `Mega_Bezel/Presets/Experimental/Guest-SlotMask-2.slangp` | ❌ |
| `Mega_Bezel/Presets/Experimental/Guest-SlotMask-3.slangp` | ❌ |
| `Mega_Bezel/Presets/Experimental/Tube-Effects__Night__ADV_GuestAperture.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__0__SMOOTH-ADV-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__0__SMOOTH-ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__1__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__2__ADV-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__2__ADV-NO-REFLECT.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__3__STD-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__3__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__4__STD-NO-REFLECT.slangp` | ❌ |
| `Mega_Bezel/Presets/MBZ__5__POTATO.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/CRT-Flavors/Guest-Slotmask-4K.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/CRT-Flavors/Newpixie-Clone__SMOOTH-ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/CRT-Flavors/Newpixie-Clone__STD__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/CRT-Flavors/Royale-Clone__ADV__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/CRT-Flavors/Royale-Clone__ADV-GLASS__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__ADV__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__ADV__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__POTATO__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD__DREZ-3DS-1600x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD__LCD-GRID__DREZ-3DS-400x480.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD-NO-REFLECT__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD-NO-REFLECT.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-3DS__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__ADV__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__ADV__LCD-GRID__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__ADV__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__ADV-NO-REFLECT.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__POTATO__GDV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD__DREZ-NDS-1280x1920.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD__DREZ-NDS-256x384.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD-NO-REFLECT__LCD-GRID.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD-NO-REFLECT.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen-DS__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Dual-Screen/Dual-Screen__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/FBNEO-Vertical__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Glass-BigBlur__ADV-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Glass-Minimal-Bezel-Edge__ADV-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_0__SMOOTH-ADV-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_1__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_2__ADV-GLASS.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_2__ADV-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_3__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_5__POTATO-GDV-MINI_No-BG.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Lightgun/Sinden_Border_5__POTATO-GDV-MINI.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-aeg-CTV-4800-VT-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-aeg-CTV-4800-VT-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-bang-olufsen-mx8000-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-bang-olufsen-mx8000-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-default-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-default-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-gba-gbi-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-gba-gbi-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-jvc-d-series-AV-36D501-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-jvc-d-series-AV-36D501-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-jvc-professional-TM-H1950CG-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-jvc-professional-TM-H1950CG-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sammy-atomiswave-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sammy-atomiswave-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sega-virtua-fighter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sega-virtua-fighter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-1910-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-1910-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-20L4-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-20L4-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-2730-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-sony-pvm-2730-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-toshiba-microfilter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-toshiba-microfilter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-viewsonic-A90f+-hdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/crt-sony-megatron-viewsonic-A90f+-sdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-aeg-CTV-4800-VT-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-aeg-CTV-4800-VT-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-bang-olufsen-mx8000-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-bang-olufsen-mx8000-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-default-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-default-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-gba-gbi-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-gba-gbi-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-jvc-d-series-AV-36D501-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-jvc-d-series-AV-36D501-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-jvc-professional-TM-H1950CG-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-jvc-professional-TM-H1950CG-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sammy-atomiswave-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sammy-atomiswave-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sega-virtua-fighter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sega-virtua-fighter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-1910-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-1910-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-20L4-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-20L4-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-2730-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-sony-pvm-2730-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-toshiba-microfilter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-toshiba-microfilter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-viewsonic-A90f+-hdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/crt-sony-megatron-viewsonic-A90f+-sdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/shaders/crt-sony-megatron-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/shaders/crt-sony-megatron-ntsc-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/shaders/crt-sony-megatron-ntsc-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV-SCREEN-ONLY/shaders/crt-sony-megatron-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/shaders/crt-sony-megatron-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/shaders/crt-sony-megatron-ntsc-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/shaders/crt-sony-megatron-ntsc-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/ADV/shaders/crt-sony-megatron-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-aeg-CTV-4800-VT-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-aeg-CTV-4800-VT-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-bang-olufsen-mx8000-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-bang-olufsen-mx8000-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-default-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-default-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-gba-gbi-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-gba-gbi-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-jvc-d-series-AV-36D501-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-jvc-d-series-AV-36D501-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-jvc-professional-TM-H1950CG-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-jvc-professional-TM-H1950CG-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sammy-atomiswave-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sammy-atomiswave-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sega-virtua-fighter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sega-virtua-fighter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-1910-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-1910-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-20L4-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-20L4-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-2730-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-sony-pvm-2730-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-toshiba-microfilter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-toshiba-microfilter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-viewsonic-A90f+-hdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/crt-sony-megatron-viewsonic-A90f+-sdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-aeg-CTV-4800-VT-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-aeg-CTV-4800-VT-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-bang-olufsen-mx8000-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-bang-olufsen-mx8000-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-default-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-default-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-gba-gbi-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-gba-gbi-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-jvc-d-series-AV-36D501-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-jvc-d-series-AV-36D501-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-jvc-professional-TM-H1950CG-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-jvc-professional-TM-H1950CG-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sammy-atomiswave-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sammy-atomiswave-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sega-virtua-fighter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sega-virtua-fighter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-1910-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-1910-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-20L4-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-20L4-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-2730-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-sony-pvm-2730-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-toshiba-microfilter-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-toshiba-microfilter-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-viewsonic-A90f+-hdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/crt-sony-megatron-viewsonic-A90f+-sdr.slangp` | ✅ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/shaders/crt-sony-megatron-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/shaders/crt-sony-megatron-ntsc-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/shaders/crt-sony-megatron-ntsc-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD-SCREEN-ONLY/shaders/crt-sony-megatron-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/shaders/crt-sony-megatron-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/shaders/crt-sony-megatron-ntsc-hdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/shaders/crt-sony-megatron-ntsc-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Megatron/STD/shaders/crt-sony-megatron-sdr.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/N64__SMOOTH-ADV__DREZ-480p.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/N64__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/NoScanlines__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Reflect-Only__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Reflect-Only__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__1__Antialias__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__1__Antialias.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__2__Default__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__2__Default.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__3__Extra-Smooth__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__3__Extra-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__4__Super-Smooth__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_0__SMOOTH__4__Super-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_2__ADV-SCREEN-ONLY__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_2__ADV-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_2__ADV-SCREEN-ONLY-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_2__ADV-SCREEN-ONLY-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_4__STD-SCREEN-ONLY__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_4__STD-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_4__STD-SCREEN-ONLY-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_4__STD-SCREEN-ONLY-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_5__POTATO__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_5__POTATO.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_5__POTATO-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Default/_5__POTATO-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__1__Antialias__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__1__Antialias.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__2__Default__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__2__Default.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__3__Extra-Smooth__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__3__Extra-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__4__Super-Smooth__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_0__SMOOTH__4__Super-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_2__ADV-SCREEN-ONLY__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_2__ADV-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_2__ADV-SCREEN-ONLY-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_2__ADV-SCREEN-ONLY-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_4__STD-SCREEN-ONLY__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_4__STD-SCREEN-ONLY.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_4__STD-SCREEN-ONLY-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_4__STD-SCREEN-ONLY-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_5__POTATO__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_5__POTATO.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_5__POTATO-SUPER-XBR__NTSC.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Screen-Only/Max-Int-Scale/_5__POTATO-SUPER-XBR.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/SharpPixels__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/ADV_1_No-Smoothing.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_1_Antialias.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_2_Default.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_3_Extra-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_4_Super-Smooth-Clear.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_4_Super-Smooth.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Smoothed/SMOOTH-ADV_5_Super-Smooth-Big-Scanlines.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Tube-Effects__Day__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Tube-Effects__Night__ADV.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Vector/Vector-BW-HighResMode__STD.slangp` | ❌ |
| `Mega_Bezel/Presets/Variations/Vector/Vector-Color-HighResMode__STD.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-00-Content-Dir.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-01-Core.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-02-Game.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-03-VideoDriver.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-04-CoreRequestedRotation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-05-VideoAllowCoreRotation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-06-VideoUserRotation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-07-VideoFinalRotation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-08-ScreenOrientation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-09-ViewportAspectOrientation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-10-CoreAspectOrientation.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-11-PresetDir.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-12-PresetName.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-13-VideoDriverPresetExtension.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/Preset-14-VideoDriverShaderExtension.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-00_$CONTENT-DIR$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-00_example-content_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-01_$CORE$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-01_image display_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-02_$GAME$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-02_Example-Image_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-03_$VID-DRV$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-03_glcore_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-04_$CORE-REQ-ROT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-04_CORE-REQ-ROT-0_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-05_$VID-ALLOW-CORE-ROT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-05_VID-ALLOW-CORE-ROT-OFF_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-06_$VID-USER-ROT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-06_VID-USER-ROT-0_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-07_$VID-FINAL-ROT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-07_VID-FINAL-ROT-0_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-08_$SCREEN-ORIENT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-08_SCREEN-ORIENT-0_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-09_$VIEW-ASPECT-ORIENT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-09_VIEW-ASPECT-ORIENT-HORZ_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-10_$CORE-ASPECT-ORIENT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-10_CORE-ASPECT-ORIENT-HORZ_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-11_$PRESET-DIR$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-11_wildcard-examples_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-12_$PRESET$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-12_Preset-12-PresetName_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-13_$VID-DRV-PRESET-EXT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-13_slangp_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-14_$VID-DRV-SHADER-EXT$_.slangp` | ✅ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-14_slang_.slangp` | ❌ |
| `Mega_Bezel/resource/wildcard-examples/referenced-presets/Ref-Base.slangp` | ❌ |
| `Mega_Bezel/shaders/hyllian/crt-super-xbr/crt-super-xbr.slangp` | ✅ |
| `scanline-classic/presets/uhd-4k-sdr/arcade/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/aaa-generic-ntsc-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/aaa-generic-ntscj-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/aaa-generic-pal-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/gen.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/md-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/md.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/sfc_sfcjr.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/snes-br.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/snes-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/consumer/snes_snesmini.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntsc-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntscj-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntscj-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntscj-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntscj-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntsc-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntsc-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-ntsc-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-pal-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/aaa-generic-pal-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/gen.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/md-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/md.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/sfc-composite_sfcjr.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/sfc-rf_sfcjr-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/sfc.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/sfc-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-br.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-composite_snesmini.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-eu-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-gb-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes-rf_snesmini-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-sdr/professional/snes.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/arcade/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/aaa-generic-ntsc-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/aaa-generic-ntscj-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/aaa-generic-pal-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/gen.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/md-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/md.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/sfc_sfcjr.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/snes-br.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/snes-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/consumer/snes_snesmini.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntsc-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntscj-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntscj-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntscj-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntscj-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntsc-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntsc-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-ntsc-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-pal-composite.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/aaa-generic-pal-rgb.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/gen.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/md-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/md.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/neogeo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/sfc-composite_sfcjr.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/sfc-rf_sfcjr-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/sfc.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/sfc-svideo.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-br.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-composite_snesmini.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-eu-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-eu.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-gb-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes-rf_snesmini-rf.slangp` | ❌ |
| `scanline-classic/presets/uhd-4k-wcg/professional/snes.slangp` | ❌ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-aperture.slangp` | ✅ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-easymode.slangp` | ✅ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-gdv-mini.slangp` | ✅ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-geom.slangp` | ✅ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-lottes.slangp` | ✅ |
| `uborder/base_presets/koko-ambi/koko-ambi-crt-nobody.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-aperture.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-easymode.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-gdv-min.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-geom.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-hyllian-sinc.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-lottes.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-pi.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/crt-sines.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/fakelottes.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/crt/phosphor-persistence.slangp` | ❌ |
| `uborder/base_presets/uborder-bezel-reflections/handheld/uborder-bezel-reflections-dot.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/handheld/uborder-bezel-reflections-lcd-grid-v2.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/uborder-bezel-reflections-crt-guest-advanced-hd.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/uborder-bezel-reflections-crt-guest-advanced-ntsc.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/uborder-bezel-reflections-crt-guest-advanced.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/uborder-bezel-reflections-crt-nobody.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/uborder-bezel-reflections-newpixie-crt.slangp` | ✅ |
| `uborder/base_presets/uborder-bezel-reflections/vector/uborder-bezel-reflections-vector.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-aperture.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-easymode.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-gdv-mini.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-geom.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-hyllian-sinc.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-lottes.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-pi.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/crt-sines.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/fakelottes.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/crt/phosphor-persistence.slangp` | ❌ |
| `uborder/base_presets/uborder-koko-ambi/handheld/dot.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/handheld/lcd-grid-v2.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/uborder-koko-ambi-crt-guest-advanced-hd.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/uborder-koko-ambi-crt-guest-advanced-ntsc.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/uborder-koko-ambi-crt-guest-advanced.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/uborder-koko-ambi-crt-nobody.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/uborder-koko-ambi-newpixie-crt.slangp` | ✅ |
| `uborder/base_presets/uborder-koko-ambi/vector/vector.slangp` | ✅ |
| `uborder/koko-ambi-crt.slangp` | ❌ |
| `uborder/preset_tools/append-uborder-bezel-reflections.slangp` | ✅ |
| `uborder/preset_tools/append-uborder-koko-ambi.slangp` | ✅ |
| `uborder/preset_tools/prepend-uborder-koko-ambi.slangp` | ✅ |
| `uborder/shaders/support_shaders/koko-ambi-standalone/koko-ambi.slangp` | ✅ |
| `uborder/uborder-bezel-reflections.slangp` | ❌ |
| `uborder/uborder-koko-ambi.slangp` | ❌ |

</details>

<details>
<summary><strong>blurs</strong> (17/17)</summary>

| Preset | Status |
|--------|--------|
| `dual_filter_2_pass.slangp` | ✅ |
| `dual_filter_4_pass.slangp` | ✅ |
| `dual_filter_6_pass.slangp` | ✅ |
| `dual_filter_bloom_fastest.slangp` | ✅ |
| `dual_filter_bloom_fast.slangp` | ✅ |
| `dual_filter_bloom.slangp` | ✅ |
| `gauss_4tap.slangp` | ✅ |
| `gaussian_blur_2_pass-sharp.slangp` | ✅ |
| `gaussian_blur_2_pass.slangp` | ✅ |
| `gaussian_blur-sharp.slangp` | ✅ |
| `gaussian_blur.slangp` | ✅ |
| `gizmo-composite-blur.slangp` | ✅ |
| `kawase_blur_5pass.slangp` | ✅ |
| `kawase_blur_9pass.slangp` | ✅ |
| `kawase_glow.slangp` | ✅ |
| `sharpsmoother.slangp` | ✅ |
| `smart-blur.slangp` | ✅ |

</details>

<details>
<summary><strong>border</strong> (43/43)</summary>

| Preset | Status |
|--------|--------|
| `ambient-glow.slangp` | ✅ |
| `autocrop-koko.slangp` | ✅ |
| `average_fill.slangp` | ✅ |
| `bigblur.slangp` | ✅ |
| `blur_fill.slangp` | ✅ |
| `blur_fill_stronger_blur.slangp` | ✅ |
| `blur_fill_weaker_blur.slangp` | ✅ |
| `effect-border-iq.slangp` | ✅ |
| `gameboy-player/gameboy-player-crt-easymode.slangp` | ✅ |
| `gameboy-player/gameboy-player-crt-geom-1x.slangp` | ✅ |
| `gameboy-player/gameboy-player-crt-royale.slangp` | ✅ |
| `gameboy-player/gameboy-player-gba-color+crt-easymode.slangp` | ✅ |
| `gameboy-player/gameboy-player-gba-color.slangp` | ✅ |
| `gameboy-player/gameboy-player.slangp` | ✅ |
| `gameboy-player/gameboy-player-tvout-gba-color+interlacing.slangp` | ✅ |
| `gameboy-player/gameboy-player-tvout-gba-color.slangp` | ✅ |
| `gameboy-player/gameboy-player-tvout+interlacing.slangp` | ✅ |
| `gameboy-player/gameboy-player-tvout.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gba+crt-consumer.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gba+dot.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gba+lcd-grid-v2.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gba.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gb+crt-consumer.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gb+dot.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gb+lcd-grid-v2.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gb.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gg+crt-consumer.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gg+dot.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gg+lcd-grid-v2.slangp` | ✅ |
| `handheld-nebula/handheld-nebula-gg.slangp` | ✅ |
| `imgborder.slangp` | ✅ |
| `lightgun-border.slangp` | ✅ |
| `sgba/sgba-gba-color.slangp` | ✅ |
| `sgba/sgba.slangp` | ✅ |
| `sgba/sgba-tvout-gba-color+interlacing.slangp` | ✅ |
| `sgba/sgba-tvout+interlacing.slangp` | ✅ |
| `sgb/sgb+crt-easymode.slangp` | ✅ |
| `sgb/sgb-crt-geom-1x.slangp` | ✅ |
| `sgb/sgb-crt-royale.slangp` | ✅ |
| `sgb/sgb-gbc-color.slangp` | ✅ |
| `sgb/sgb-gbc-color-tvout+interlacing.slangp` | ✅ |
| `sgb/sgb.slangp` | ✅ |
| `sgb/sgb-tvout+interlacing.slangp` | ✅ |

</details>

<details>
<summary><strong>cel</strong> (3/3)</summary>

| Preset | Status |
|--------|--------|
| `advcartoon.slangp` | ✅ |
| `MMJ_Cel_Shader_MP.slangp` | ✅ |
| `MMJ_Cel_Shader.slangp` | ✅ |

</details>

<details>
<summary><strong>crt</strong> (109/117)</summary>

| Preset | Status |
|--------|--------|
| `advanced_crt_whkrmrgks0.slangp` | ✅ |
| `cathode-retro_no-signal.slangp` | ✅ |
| `crt-1tap-bloom_fast.slangp` | ✅ |
| `crt-1tap-bloom.slangp` | ✅ |
| `crt-1tap.slangp` | ✅ |
| `crt-aperture.slangp` | ✅ |
| `crt-beans-fast.slangp` | ✅ |
| `crt-beans-rgb.slangp` | ✅ |
| `crt-beans-vga.slangp` | ✅ |
| `crt-blurPi-sharp.slangp` | ✅ |
| `crt-blurPi-soft.slangp` | ✅ |
| `crt-caligari.slangp` | ✅ |
| `crt-cgwg-fast.slangp` | ✅ |
| `crt-consumer-1w-ntsc-XL.slangp` | ✅ |
| `crt-consumer.slangp` | ✅ |
| `crt-CreativeForce-Arcade.slangp` | ✅ |
| `crt-CreativeForce-SharpSmooth.slangp` | ✅ |
| `crt-Cyclon.slangp` | ✅ |
| `crt-easymode-halation.slangp` | ✅ |
| `crt-easymode.slangp` | ✅ |
| `crt-effects/analog-service-menu.slangp` | ✅ |
| `crt-effects/crt-black_crush-koko.slangp` | ✅ |
| `crt-effects/crt-resswitch-glitch-koko.slangp` | ✅ |
| `crt-effects/glow_trails.slangp` | ❌ |
| `crt-effects/phosphorlut.slangp` | ✅ |
| `crt-effects/phosphor-persistence.slangp` | ❌ |
| `crt-effects/raster_bloom.slangp` | ✅ |
| `crt-effects/ray_traced_curvature_append.slangp` | ✅ |
| `crt-effects/ray_traced_curvature.slangp` | ❌ |
| `crt-effects/vector-glow-alt-render.slangp` | ✅ |
| `crt-effects/vector-glow.slangp` | ✅ |
| `crt-frutbunn.slangp` | ✅ |
| `crt-gdv-mini.slangp` | ✅ |
| `crt-gdv-mini-ultra-trinitron.slangp` | ✅ |
| `crt-geom-deluxe.slangp` | ✅ |
| `crt-geom-mini.slangp` | ✅ |
| `crt-geom.slangp` | ✅ |
| `crt-geom-tate.slangp` | ✅ |
| `crtglow_gauss.slangp` | ✅ |
| `crtglow_lanczos.slangp` | ✅ |
| `crt-guest-advanced-fastest.slangp` | ✅ |
| `crt-guest-advanced-fast.slangp` | ✅ |
| `crt-guest-advanced-hd.slangp` | ✅ |
| `crt-guest-advanced-ntsc.slangp` | ✅ |
| `crt-guest-advanced.slangp` | ✅ |
| `crt-hyllian-3d.slangp` | ✅ |
| `crt-hyllian-fast.slangp` | ✅ |
| `crt-hyllian-ntsc-rainbow.slangp` | ✅ |
| `crt-hyllian-ntsc.slangp` | ✅ |
| `crt-hyllian-sinc-composite.slangp` | ✅ |
| `crt-hyllian.slangp` | ✅ |
| `crt-interlaced-halation.slangp` | ✅ |
| `crt-lottes-fast.slangp` | ✅ |
| `crt-lottes-multipass-glow.slangp` | ✅ |
| `crt-lottes-multipass.slangp` | ✅ |
| `crt-lottes.slangp` | ✅ |
| `crt-mattias.slangp` | ✅ |
| `crt-maximus-royale-fast-mode.slangp` | ✅ |
| `crt-maximus-royale-half-res-mode.slangp` | ✅ |
| `crt-maximus-royale.slangp` | ✅ |
| `crt-nes-mini.slangp` | ✅ |
| `crt-nobody.slangp` | ✅ |
| `crt-pi.slangp` | ✅ |
| `crt-pocket.slangp` | ✅ |
| `crt-potato-BVM.slangp` | ✅ |
| `crt-potato-cool.slangp` | ✅ |
| `crt-potato-warm.slangp` | ✅ |
| `crt-royale-fake-bloom-intel.slangp` | ✅ |
| `crt-royale-fake-bloom.slangp` | ✅ |
| `crt-royale-fast.slangp` | ✅ |
| `crt-royale-intel.slangp` | ✅ |
| `crt-royale.slangp` | ✅ |
| `crt-simple.slangp` | ✅ |
| `crtsim.slangp` | ✅ |
| `crt-sines.slangp` | ✅ |
| `crt-slangtest-cubic.slangp` | ✅ |
| `crt-slangtest-lanczos.slangp` | ✅ |
| `crt-super-xbr.slangp` | ✅ |
| `crt-torridgristle.slangp` | ✅ |
| `crt-yah.single-pass.slangp` | ✅ |
| `crt-yah.slangp` | ❌ |
| `crt-yo6-flat-trinitron-tv.slangp` | ✅ |
| `crt-yo6-KV-21CL10B.slangp` | ✅ |
| `crt-yo6-KV-M1420B-sharp.slangp` | ✅ |
| `crt-yo6-KV-M1420B.slangp` | ✅ |
| `fake-crt-geom-potato.slangp` | ✅ |
| `fake-crt-geom.slangp` | ✅ |
| `fakelottes.slangp` | ✅ |
| `gizmo-crt.slangp` | ✅ |
| `gizmo-slotmask-crt.slangp` | ✅ |
| `GritsScanlines.slangp` | ✅ |
| `gtu-v050.slangp` | ✅ |
| `lottesRVM.slangp` | ✅ |
| `mame_hlsl.slangp` | ✅ |
| `metacrt.slangp` | ✅ |
| `newpixie-crt.slangp` | ✅ |
| `newpixie-mini.slangp` | ✅ |
| `shaders/cathode-retro/signal_test.slangp` | ✅ |
| `shaders/crt-yah/crt-yah.single-pass.slangp` | ✅ |
| `shaders/crt-yah/crt-yah.slangp` | ❌ |
| `shaders/crt-yah/presets/base-lite.slangp` | ❌ |
| `shaders/crt-yah/presets/base-medium.slangp` | ❌ |
| `shaders/crt-yah/presets/base-strong.slangp` | ❌ |
| `shaders/crt-yah/presets/pure-mask.slangp` | ❌ |
| `shaders/mame_hlsl/shaders/old/mame_hlsl.slangp` | ✅ |
| `simple-crt-fxaa.slangp` | ✅ |
| `simple-crt.slangp` | ✅ |
| `tvout-tweaks.slangp` | ✅ |
| `vt220.slangp` | ✅ |
| `yee64.slangp` | ✅ |
| `yeetron.slangp` | ✅ |
| `zfast-crt-composite.slangp` | ✅ |
| `zfast-crt-curvature.slangp` | ✅ |
| `zfast-crt-geo.slangp` | ✅ |
| `zfast-crt-geo-svideo.slangp` | ✅ |
| `zfast-crt-hdmask.slangp` | ✅ |
| `zfast-crt.slangp` | ✅ |

</details>

<details>
<summary><strong>deblur</strong> (2/2)</summary>

| Preset | Status |
|--------|--------|
| `deblur-luma.slangp` | ✅ |
| `deblur.slangp` | ✅ |

</details>

<details>
<summary><strong>deinterlacing</strong> (11/11)</summary>

| Preset | Status |
|--------|--------|
| `bob-deinterlacing.slangp` | ✅ |
| `deinterlace.slangp` | ✅ |
| `gtu-v050-deinterlaced.slangp` | ✅ |
| `interpolation-deinterlacer.slangp` | ✅ |
| `motion-adaptive-deinterlacing.slangp` | ✅ |
| `nnedi3-nns128-deinterlacing.slangp` | ✅ |
| `nnedi3-nns16-deinterlacing.slangp` | ✅ |
| `nnedi3-nns256-deinterlacing.slangp` | ✅ |
| `nnedi3-nns32-deinterlacing.slangp` | ✅ |
| `nnedi3-nns64-deinterlacing.slangp` | ✅ |
| `reinterlacing.slangp` | ✅ |

</details>

<details>
<summary><strong>denoisers</strong> (7/7)</summary>

| Preset | Status |
|--------|--------|
| `bilateral-2p.slangp` | ✅ |
| `bilateral.slangp` | ✅ |
| `crt-fast-bilateral-super-xbr.slangp` | ✅ |
| `fast-bilateral.slangp` | ✅ |
| `median_3x3.slangp` | ✅ |
| `median_5x5.slangp` | ✅ |
| `slow-bilateral.slangp` | ✅ |

</details>

<details>
<summary><strong>dithering</strong> (17/17)</summary>

| Preset | Status |
|--------|--------|
| `bayer_4x4.slangp` | ✅ |
| `bayer-matrix-dithering.slangp` | ✅ |
| `blue_noise_dynamic_4Bit.slangp` | ✅ |
| `blue_noise_dynamic_monochrome.slangp` | ✅ |
| `blue_noise.slangp` | ✅ |
| `cbod_v1.slangp` | ✅ |
| `checkerboard-dedither.slangp` | ✅ |
| `gdapt.slangp` | ✅ |
| `gendither.slangp` | ✅ |
| `g-sharp_resampler.slangp` | ✅ |
| `jinc2-dedither.slangp` | ✅ |
| `mdapt.slangp` | ✅ |
| `ps1-dedither-boxblur.slangp` | ✅ |
| `ps1-dedither-comparison.slangp` | ✅ |
| `ps1-dither.slangp` | ✅ |
| `sgenpt-mix-multipass.slangp` | ✅ |
| `sgenpt-mix.slangp` | ✅ |

</details>

<details>
<summary><strong>downsample</strong> (59/59)</summary>

| Preset | Status |
|--------|--------|
| `drez_1x.slangp` | ✅ |
| `drez/drez_3ds_x-400_y-480.slangp` | ✅ |
| `drez/drez_nds_x-256_y-384.slangp` | ✅ |
| `drez/drez_ps2_x-1024_y-448.slangp` | ✅ |
| `drez/drez_ps2_x-1024_y-896.slangp` | ✅ |
| `drez/drez_ps2_x-1280_y-448.slangp` | ✅ |
| `drez/drez_ps2_x-1280_y-896.slangp` | ✅ |
| `drez/drez_ps2_x-2560_y-1792.slangp` | ✅ |
| `drez/drez_ps2_x-5120_y-3584.slangp` | ✅ |
| `drez/drez_ps2_x-512_y-448.slangp` | ✅ |
| `drez/drez_ps2_x-640_y-448.slangp` | ✅ |
| `drez/drez_psp_x-480_y-272.slangp` | ✅ |
| `drez/drez_psp_x-960_y-544.slangp` | ✅ |
| `drez/drez_psp_x-viewport_y-272.slangp` | ✅ |
| `drez/drez_psp_x-viewport_y-544.slangp` | ✅ |
| `drez/drez_x-1440_y-1080.slangp` | ✅ |
| `drez/drez_x-1920_y-1080.slangp` | ✅ |
| `drez/drez_x-1x_y-240.slangp` | ✅ |
| `drez/drez_x-320_y-224.slangp` | ✅ |
| `drez/drez_x-320_y-240.slangp` | ✅ |
| `drez/drez_x-640_y-480.slangp` | ✅ |
| `drez/drez_x-viewport_y-224.slangp` | ✅ |
| `drez/drez_x-viewport_y-240.slangp` | ✅ |
| `drez/drez_x-viewport_y-320.slangp` | ✅ |
| `drez/drez_x-viewport_y-480.slangp` | ✅ |
| `mixed-res/2x/mixed-res-bicubic.slangp` | ✅ |
| `mixed-res/2x/mixed-res-bilinear.slangp` | ✅ |
| `mixed-res/2x/mixed-res-jinc2.slangp` | ✅ |
| `mixed-res/2x/mixed-res-lanczos3.slangp` | ✅ |
| `mixed-res/2x/mixed-res-nnedi3-luma.slangp` | ✅ |
| `mixed-res/2x/mixed-res-reverse-aa.slangp` | ✅ |
| `mixed-res/2x/mixed-res-spline16.slangp` | ✅ |
| `mixed-res/2x/mixed-res-super-xbr-film-full.slangp` | ✅ |
| `mixed-res/2x/mixed-res-super-xbr-film.slangp` | ✅ |
| `mixed-res/2x/mixed-res-super-xbr.slangp` | ✅ |
| `mixed-res/3x/mixed-res-bicubic.slangp` | ✅ |
| `mixed-res/3x/mixed-res-bilinear.slangp` | ✅ |
| `mixed-res/3x/mixed-res-jinc2.slangp` | ✅ |
| `mixed-res/3x/mixed-res-lanczos3.slangp` | ✅ |
| `mixed-res/3x/mixed-res-nnedi3-luma.slangp` | ✅ |
| `mixed-res/3x/mixed-res-reverse-aa.slangp` | ✅ |
| `mixed-res/3x/mixed-res-spline16.slangp` | ✅ |
| `mixed-res/3x/mixed-res-super-xbr-film-full.slangp` | ✅ |
| `mixed-res/3x/mixed-res-super-xbr-film.slangp` | ✅ |
| `mixed-res/3x/mixed-res-super-xbr.slangp` | ✅ |
| `mixed-res-4x-crt-hyllian.slangp` | ✅ |
| `mixed-res-4x-jinc2.slangp` | ✅ |
| `mixed-res/4x/mixed-res-bicubic.slangp` | ✅ |
| `mixed-res/4x/mixed-res-bilinear.slangp` | ✅ |
| `mixed-res/4x/mixed-res-jinc2.slangp` | ✅ |
| `mixed-res/4x/mixed-res-lanczos3.slangp` | ✅ |
| `mixed-res/4x/mixed-res-nnedi3-luma.slangp` | ✅ |
| `mixed-res/4x/mixed-res-reverse-aa.slangp` | ✅ |
| `mixed-res/4x/mixed-res-spline16.slangp` | ✅ |
| `mixed-res/4x/mixed-res-super-xbr-film-full.slangp` | ✅ |
| `mixed-res/4x/mixed-res-super-xbr-film.slangp` | ✅ |
| `mixed-res/4x/mixed-res-super-xbr.slangp` | ✅ |
| `mixed-res/hooks/mixed-res-4x-append.slangp` | ✅ |
| `mixed-res/hooks/mixed-res-4x-prepend.slangp` | ✅ |

</details>

<details>
<summary><strong>edge-smoothing</strong> (95/95)</summary>

| Preset | Status |
|--------|--------|
| `cleanEdge/cleanEdge-scale.slangp` | ✅ |
| `ddt/3-point.slangp` | ✅ |
| `ddt/cut.slangp` | ✅ |
| `ddt/ddt-extended.slangp` | ✅ |
| `ddt/ddt-jinc-linear.slangp` | ✅ |
| `ddt/ddt-jinc.slangp` | ✅ |
| `ddt/ddt.slangp` | ✅ |
| `ddt/ddt-xbr-lv1.slangp` | ✅ |
| `eagle/2xsai-fix-pixel-shift.slangp` | ✅ |
| `eagle/2xsai.slangp` | ✅ |
| `eagle/super-2xsai-fix-pixel-shift.slangp` | ✅ |
| `eagle/super-2xsai.slangp` | ✅ |
| `eagle/supereagle.slangp` | ✅ |
| `fsr/fsr-easu.slangp` | ✅ |
| `fsr/fsr.slangp` | ✅ |
| `fsr/smaa+fsr.slangp` | ✅ |
| `hqx/hq2x-halphon.slangp` | ✅ |
| `hqx/hq2x.slangp` | ✅ |
| `hqx/hq3x.slangp` | ✅ |
| `hqx/hq4x.slangp` | ✅ |
| `nedi/fast-bilateral-nedi.slangp` | ✅ |
| `nedi/nedi-hybrid-sharper.slangp` | ✅ |
| `nedi/nedi-hybrid.slangp` | ✅ |
| `nedi/nedi-sharper.slangp` | ✅ |
| `nedi/nedi.slangp` | ✅ |
| `nedi/presets/bilateral-variant2.slangp` | ✅ |
| `nedi/presets/bilateral-variant3.slangp` | ✅ |
| `nedi/presets/bilateral-variant4.slangp` | ✅ |
| `nedi/presets/bilateral-variant5.slangp` | ✅ |
| `nedi/presets/bilateral-variant6.slangp` | ✅ |
| `nedi/presets/bilateral-variant7.slangp` | ✅ |
| `nedi/presets/bilateral-variant.slangp` | ✅ |
| `nnedi3/nnedi3-nns16-2x-luma.slangp` | ✅ |
| `nnedi3/nnedi3-nns16-2x-rgb.slangp` | ✅ |
| `nnedi3/nnedi3-nns16-4x-luma.slangp` | ✅ |
| `nnedi3/nnedi3-nns32-2x-rgb-nns32-4x-luma.slangp` | ✅ |
| `nnedi3/nnedi3-nns32-4x-rgb.slangp` | ✅ |
| `nnedi3/nnedi3-nns64-2x-nns32-4x-nns16-8x-rgb.slangp` | ✅ |
| `nnedi3/nnedi3-nns64-2x-nns32-4x-rgb.slangp` | ✅ |
| `omniscale/omniscale-legacy.slangp` | ✅ |
| `omniscale/omniscale.slangp` | ✅ |
| `sabr/sabr-hybrid-deposterize.slangp` | ✅ |
| `sabr/sabr.slangp` | ✅ |
| `scalefx/scalefx-9x.slangp` | ✅ |
| `scalefx/scalefx-hybrid.slangp` | ✅ |
| `scalefx/scalefx+rAA.slangp` | ✅ |
| `scalefx/scalefx.slangp` | ✅ |
| `scalefx/shaders/old/scalefx-9x.slangp` | ✅ |
| `scalefx/shaders/old/scalefx.slangp` | ✅ |
| `scalehq/2xScaleHQ.slangp` | ✅ |
| `scalehq/4xScaleHQ.slangp` | ✅ |
| `scalenx/epx.slangp` | ✅ |
| `scalenx/mmpx-ex.slangp` | ✅ |
| `scalenx/mmpx.slangp` | ✅ |
| `scalenx/scale2xplus.slangp` | ✅ |
| `scalenx/scale2xSFX.slangp` | ✅ |
| `scalenx/scale2x.slangp` | ✅ |
| `scalenx/scale3x.slangp` | ✅ |
| `xbr/hybrid-jinc2-xbr-lv2.slangp` | ✅ |
| `xbr/other presets/2xBR-lv1-multipass.slangp` | ✅ |
| `xbr/other presets/4xbr-hybrid-crt.slangp` | ✅ |
| `xbr/other presets/super-2xbr-3d-2p.slangp` | ✅ |
| `xbr/other presets/super-2xbr-3d-3p-smoother.slangp` | ✅ |
| `xbr/other presets/super-4xbr-3d-4p.slangp` | ✅ |
| `xbr/other presets/super-4xbr-3d-6p-smoother.slangp` | ✅ |
| `xbr/other presets/super-8xbr-3d-6p.slangp` | ✅ |
| `xbr/other presets/xbr-hybrid.slangp` | ✅ |
| `xbr/other presets/xbr-lv1-standalone.slangp` | ✅ |
| `xbr/other presets/xbr-lv2-hd.slangp` | ✅ |
| `xbr/other presets/xbr-lv2-multipass.slangp` | ✅ |
| `xbr/other presets/xbr-lv2-standalone.slangp` | ✅ |
| `xbr/other presets/xbr-lv3-9x-multipass.slangp` | ✅ |
| `xbr/other presets/xbr-lv3-9x-standalone.slangp` | ✅ |
| `xbr/other presets/xbr-lv3-multipass.slangp` | ✅ |
| `xbr/other presets/xbr-lv3-standalone.slangp` | ✅ |
| `xbr/other presets/xbr-mlv4-multipass.slangp` | ✅ |
| `xbr/super-xbr-fast.slangp` | ✅ |
| `xbr/super-xbr.slangp` | ✅ |
| `xbr/xbr-lv2-sharp.slangp` | ✅ |
| `xbr/xbr-lv2.slangp` | ✅ |
| `xbr/xbr-lv3-sharp.slangp` | ✅ |
| `xbr/xbr-lv3.slangp` | ✅ |
| `xbrz/2xbrz-linear.slangp` | ✅ |
| `xbrz/4xbrz-linear.slangp` | ✅ |
| `xbrz/5xbrz-linear.slangp` | ✅ |
| `xbrz/6xbrz-linear.slangp` | ✅ |
| `xbrz/xbrz-freescale-multipass.slangp` | ✅ |
| `xbrz/xbrz-freescale.slangp` | ✅ |
| `xsal/2xsal-level2-crt.slangp` | ✅ |
| `xsal/2xsal.slangp` | ✅ |
| `xsal/4xsal-level2-crt.slangp` | ✅ |
| `xsal/4xsal-level2-hq.slangp` | ✅ |
| `xsal/4xsal-level2.slangp` | ✅ |
| `xsoft/4xsoftSdB.slangp` | ✅ |
| `xsoft/4xsoft.slangp` | ✅ |

</details>

<details>
<summary><strong>film</strong> (2/2)</summary>

| Preset | Status |
|--------|--------|
| `film-grain.slangp` | ✅ |
| `technicolor.slangp` | ✅ |

</details>

<details>
<summary><strong>gpu</strong> (3/3)</summary>

| Preset | Status |
|--------|--------|
| `3dfx_4x1.slangp` | ✅ |
| `powervr2.slangp` | ✅ |
| `shaders/3dfx/old/3dfx_4x1.slangp` | ✅ |

</details>

<details>
<summary><strong>handheld</strong> (67/67)</summary>

| Preset | Status |
|--------|--------|
| `agb001.slangp` | ✅ |
| `ags001.slangp` | ✅ |
| `authentic_gbc_fast.slangp` | ✅ |
| `authentic_gbc_single_pass.slangp` | ✅ |
| `authentic_gbc.slangp` | ✅ |
| `bevel.slangp` | ✅ |
| `color-mod/dslite-color.slangp` | ✅ |
| `color-mod/gba-color.slangp` | ✅ |
| `color-mod/gbc-color.slangp` | ✅ |
| `color-mod/gbc-gambatte-color.slangp` | ✅ |
| `color-mod/gbMicro-color.slangp` | ✅ |
| `color-mod/nds-color.slangp` | ✅ |
| `color-mod/NSO-gba-color.slangp` | ✅ |
| `color-mod/NSO-gbc-color.slangp` | ✅ |
| `color-mod/palm-color.slangp` | ✅ |
| `color-mod/psp-color.slangp` | ✅ |
| `color-mod/sp101-color.slangp` | ✅ |
| `color-mod/SwitchOLED-color.slangp` | ✅ |
| `color-mod/vba-color.slangp` | ✅ |
| `console-border/dmg.slangp` | ✅ |
| `console-border/gba-agb001-color-motionblur.slangp` | ✅ |
| `console-border/gba-ags001-color-motionblur.slangp` | ✅ |
| `console-border/gba-dmg.slangp` | ✅ |
| `console-border/gba-lcd-grid-v2.slangp` | ✅ |
| `console-border/gba.slangp` | ✅ |
| `console-border/gbc-dmg.slangp` | ✅ |
| `console-border/gbc-lcd-grid-v2.slangp` | ✅ |
| `console-border/gbc.slangp` | ✅ |
| `console-border/gb-dmg-alt.slangp` | ✅ |
| `console-border/gb-light-alt.slangp` | ✅ |
| `console-border/gb-pocket-alt.slangp` | ✅ |
| `console-border/gb-pocket.slangp` | ✅ |
| `console-border/gg.slangp` | ✅ |
| `console-border/ngpc.slangp` | ✅ |
| `console-border/psp.slangp` | ✅ |
| `dot.slangp` | ✅ |
| `ds-hybrid-sabr.slangp` | ✅ |
| `ds-hybrid-scalefx.slangp` | ✅ |
| `gameboy-advance-dot-matrix.slangp` | ✅ |
| `gameboy-color-dot-matrix.slangp` | ✅ |
| `gameboy-color-dot-matrix-white-bg.slangp` | ✅ |
| `gameboy-dark-mode.slangp` | ✅ |
| `gameboy-light-mode.slangp` | ✅ |
| `gameboy-light.slangp` | ✅ |
| `gameboy-pocket-high-contrast.slangp` | ✅ |
| `gameboy-pocket.slangp` | ✅ |
| `gameboy.slangp` | ✅ |
| `gbc-dev.slangp` | ✅ |
| `gb-palette-dmg.slangp` | ✅ |
| `gb-palette-light.slangp` | ✅ |
| `gb-palette-pocket.slangp` | ✅ |
| `lcd1x_nds.slangp` | ✅ |
| `lcd1x_psp.slangp` | ✅ |
| `lcd1x.slangp` | ✅ |
| `lcd3x.slangp` | ✅ |
| `lcd-grid.slangp` | ✅ |
| `lcd-grid-v2.slangp` | ✅ |
| `lcd-shader.slangp` | ✅ |
| `pixel_transparency.slangp` | ✅ |
| `retro-tiles.slangp` | ✅ |
| `retro-v2.slangp` | ✅ |
| `retro-v3.slangp` | ✅ |
| `sameboy-lcd.slangp` | ✅ |
| `simpletex_lcd-4k.slangp` | ✅ |
| `simpletex_lcd_720p.slangp` | ✅ |
| `simpletex_lcd.slangp` | ✅ |
| `zfast-lcd.slangp` | ✅ |

</details>

<details>
<summary><strong>hdr</strong> (4/29)</summary>

| Preset | Status |
|--------|--------|
| `crt-sony-megatron-aeg-CTV-4800-VT-hdr.slangp` | ❌ |
| `crt-sony-megatron-bang-olufsen-mx8000-hdr.slangp` | ❌ |
| `crt-sony-megatron-bang-olufsen-mx8000-sdr.slangp` | ❌ |
| `crt-sony-megatron-default-hdr-NTSC.slangp` | ✅ |
| `crt-sony-megatron-default-hdr.slangp` | ❌ |
| `crt-sony-megatron-gba-gbi-hdr.slangp` | ❌ |
| `crt-sony-megatron-jvc-d-series-AV-36D501-hdr.slangp` | ❌ |
| `crt-sony-megatron-jvc-d-series-AV-36D501-sdr.slangp` | ❌ |
| `crt-sony-megatron-jvc-professional-TM-H1950CG-hdr.slangp` | ❌ |
| `crt-sony-megatron-jvc-professional-TM-H1950CG-sdr.slangp` | ❌ |
| `crt-sony-megatron-sammy-atomiswave-hdr.slangp` | ❌ |
| `crt-sony-megatron-sammy-atomiswave-sdr.slangp` | ❌ |
| `crt-sony-megatron-saturated-hdr.slangp` | ❌ |
| `crt-sony-megatron-sega-virtua-fighter-hdr.slangp` | ❌ |
| `crt-sony-megatron-sega-virtua-fighter-sdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-1910-hdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-1910-sdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-20L4-hdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-20L4-sdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-2730-hdr-NTSC.slangp` | ✅ |
| `crt-sony-megatron-sony-pvm-2730-hdr.slangp` | ❌ |
| `crt-sony-megatron-sony-pvm-2730-sdr.slangp` | ❌ |
| `crt-sony-megatron-toshiba-microfilter-hdr.slangp` | ❌ |
| `crt-sony-megatron-toshiba-microfilter-sdr.slangp` | ❌ |
| `crt-sony-megatron-viewsonic-A90f+-hdr.slangp` | ✅ |
| `crt-sony-megatron-viewsonic-A90f+-sdr.slangp` | ✅ |
| `shaders/crt-sony-megatron-hdr.slangp` | ❌ |
| `shaders/crt-sony-megatron-sdr.slangp` | ❌ |
| `shaders/crt-sony-megatron.slangp` | ✅ |

</details>

<details>
<summary><strong>interpolation</strong> (42/42)</summary>

| Preset | Status |
|--------|--------|
| `bicubic-5-taps.slangp` | ✅ |
| `bicubic-6-taps-fast.slangp` | ✅ |
| `bicubic-fast.slangp` | ✅ |
| `bicubic.slangp` | ✅ |
| `b-spline-4-taps-fast.slangp` | ✅ |
| `b-spline-4-taps.slangp` | ✅ |
| `b-spline-fast.slangp` | ✅ |
| `catmull-rom-4-taps.slangp` | ✅ |
| `catmull-rom-5-taps.slangp` | ✅ |
| `catmull-rom-6-taps-fast.slangp` | ✅ |
| `catmull-rom-fast.slangp` | ✅ |
| `catmull-rom.slangp` | ✅ |
| `cubic-gamma-correct.slangp` | ✅ |
| `cubic.slangp` | ✅ |
| `hann-5-taps.slangp` | ✅ |
| `hermite.slangp` | ✅ |
| `jinc2-sharper.slangp` | ✅ |
| `jinc2-sharp.slangp` | ✅ |
| `jinc2.slangp` | ✅ |
| `lanczos16-AR.slangp` | ✅ |
| `lanczos2-5-taps.slangp` | ✅ |
| `lanczos2-6-taps-fast.slangp` | ✅ |
| `lanczos2-fast.slangp` | ✅ |
| `lanczos2.slangp` | ✅ |
| `lanczos3-fast.slangp` | ✅ |
| `lanczos4.slangp` | ✅ |
| `lanczos6.slangp` | ✅ |
| `lanczos8.slangp` | ✅ |
| `linear-gamma-presets/bicubic-fast.slangp` | ✅ |
| `linear-gamma-presets/b-spline-fast.slangp` | ✅ |
| `linear-gamma-presets/catmull-rom-fast.slangp` | ✅ |
| `linear-gamma-presets/lanczos2-fast.slangp` | ✅ |
| `linear-gamma-presets/lanczos3-fast.slangp` | ✅ |
| `linear-gamma-presets/spline16-fast.slangp` | ✅ |
| `linear-gamma-presets/spline36-fast.slangp` | ✅ |
| `quilez.slangp` | ✅ |
| `spline100.slangp` | ✅ |
| `spline144.slangp` | ✅ |
| `spline16-fast.slangp` | ✅ |
| `spline256.slangp` | ✅ |
| `spline36-fast.slangp` | ✅ |
| `spline64.slangp` | ✅ |

</details>

<details>
<summary><strong>linear</strong> (1/1)</summary>

| Preset | Status |
|--------|--------|
| `linear-gamma-correct.slangp` | ✅ |

</details>

<details>
<summary><strong>misc</strong> (36/36)</summary>

| Preset | Status |
|--------|--------|
| `accessibility_mods.slangp` | ✅ |
| `anti-flicker.slangp` | ✅ |
| `ascii.slangp` | ✅ |
| `bead.slangp` | ✅ |
| `chroma.slangp` | ✅ |
| `chromaticity.slangp` | ✅ |
| `cmyk-halftone-dot.slangp` | ✅ |
| `cocktail-cabinet.slangp` | ✅ |
| `colorimetry.slangp` | ✅ |
| `color-mangler.slangp` | ✅ |
| `convergence.slangp` | ✅ |
| `deband.slangp` | ✅ |
| `edge-detect.slangp` | ✅ |
| `ega.slangp` | ✅ |
| `geom-append.slangp` | ✅ |
| `geom.slangp` | ✅ |
| `glass.slangp` | ✅ |
| `grade-no-LUT.slangp` | ✅ |
| `grade.slangp` | ✅ |
| `half_res.slangp` | ✅ |
| `image-adjustment.slangp` | ✅ |
| `img_mod.slangp` | ✅ |
| `natural-vision.slangp` | ✅ |
| `night-mode.slangp` | ✅ |
| `ntsc-colors.slangp` | ✅ |
| `patchy-color.slangp` | ✅ |
| `print-resolution.slangp` | ✅ |
| `relief.slangp` | ✅ |
| `retro-palettes.slangp` | ✅ |
| `simple_color_controls.slangp` | ✅ |
| `ss-gamma-ramp.slangp` | ✅ |
| `test-pattern-append.slangp` | ✅ |
| `test-pattern-prepend.slangp` | ✅ |
| `tonemapping.slangp` | ✅ |
| `white_point.slangp` | ✅ |
| `yiq-hue-adjustment.slangp` | ✅ |

</details>

<details>
<summary><strong>motionblur</strong> (8/8)</summary>

| Preset | Status |
|--------|--------|
| `braid-rewind.slangp` | ✅ |
| `feedback.slangp` | ✅ |
| `mix_frames.slangp` | ✅ |
| `mix_frames_smart.slangp` | ✅ |
| `motionblur-blue.slangp` | ✅ |
| `motionblur-color.slangp` | ✅ |
| `motionblur-simple.slangp` | ✅ |
| `response-time.slangp` | ✅ |

</details>

<details>
<summary><strong>motion-interpolation</strong> (1/1)</summary>

| Preset | Status |
|--------|--------|
| `motion_interpolation.slangp` | ✅ |

</details>

<details>
<summary><strong>nes_raw_palette</strong> (5/5)</summary>

| Preset | Status |
|--------|--------|
| `cgwg-famicom-geom.slangp` | ✅ |
| `gtu-famicom.slangp` | ✅ |
| `ntsc-nes.slangp` | ✅ |
| `pal-r57shell-raw.slangp` | ✅ |
| `patchy-mesen-raw-palette.slangp` | ✅ |

</details>

<details>
<summary><strong>ntsc</strong> (25/25)</summary>

| Preset | Status |
|--------|--------|
| `artifact-colors.slangp` | ✅ |
| `blargg.slangp` | ✅ |
| `mame-ntsc.slangp` | ✅ |
| `ntsc-256px-composite-scanline.slangp` | ✅ |
| `ntsc-256px-composite.slangp` | ✅ |
| `ntsc-256px-svideo-scanline.slangp` | ✅ |
| `ntsc-256px-svideo.slangp` | ✅ |
| `ntsc-320px-composite-scanline.slangp` | ✅ |
| `ntsc-320px-composite.slangp` | ✅ |
| `ntsc-320px-svideo-scanline.slangp` | ✅ |
| `ntsc-320px-svideo.slangp` | ✅ |
| `ntsc-adaptive-4x.slangp` | ✅ |
| `ntsc-adaptive-old-4x.slangp` | ✅ |
| `ntsc-adaptive-old.slangp` | ✅ |
| `ntsc-adaptive.slangp` | ✅ |
| `ntsc-adaptive-tate.slangp` | ✅ |
| `ntsc-blastem.slangp` | ✅ |
| `ntsc-md-rainbows.slangp` | ✅ |
| `ntsc-simple.slangp` | ✅ |
| `ntsc-xot.slangp` | ✅ |
| `patchy-blastem.slangp` | ✅ |
| `patchy-genplusgx.slangp` | ✅ |
| `patchy-snes.slangp` | ✅ |
| `shaders/patchy-ntsc/afterglow0-update/afterglow0-update.slangp` | ✅ |
| `tiny_ntsc.slangp` | ✅ |

</details>

<details>
<summary><strong>pal</strong> (3/3)</summary>

| Preset | Status |
|--------|--------|
| `pal-r57shell-moire-only.slangp` | ✅ |
| `pal-r57shell.slangp` | ✅ |
| `pal-singlepass.slangp` | ✅ |

</details>

<details>
<summary><strong>pixel-art-scaling</strong> (22/23)</summary>

| Preset | Status |
|--------|--------|
| `aann.slangp` | ✅ |
| `bandlimit-pixel.slangp` | ✅ |
| `bilinear-adjustable.slangp` | ✅ |
| `box_filter_aa_xform.slangp` | ✅ |
| `cleanEdge-rotate.slangp` | ✅ |
| `controlled_sharpness.slangp` | ✅ |
| `edge1pixel.slangp` | ✅ |
| `edgeNpixels.slangp` | ✅ |
| `grid-blend-hybrid.slangp` | ✅ |
| `pixel_aa_fast.slangp` | ✅ |
| `pixel_aa_single_pass.slangp` | ✅ |
| `pixel_aa.slangp` | ✅ |
| `pixel_aa_xform.slangp` | ✅ |
| `pixellate.slangp` | ✅ |
| `sharp-bilinear-2x-prescale.slangp` | ✅ |
| `sharp-bilinear-scanlines.slangp` | ✅ |
| `sharp-bilinear-simple.slangp` | ✅ |
| `sharp-bilinear.slangp` | ✅ |
| `sharp-shimmerless.slangp` | ✅ |
| `smootheststep.slangp` | ✅ |
| `smuberstep.slangp` | ✅ |
| `soft-pixel-art.slangp` | ❌ |
| `uniform-nearest.slangp` | ✅ |

</details>

<details>
<summary><strong>presets</strong> (231/231)</summary>

| Preset | Status |
|--------|--------|
| `blurs/dual-bloom-filter-aa-lv2-fsr-gamma-ramp-glass.slangp` | ✅ |
| `blurs/dual-bloom-filter-aa-lv2-fsr-gamma-ramp.slangp` | ✅ |
| `blurs/dual-bloom-filter-aa-lv2-fsr.slangp` | ✅ |
| `blurs/gizmo-composite-blur-aa-lv2-fsr-gamma-ramp-gsharp-resampler.slangp` | ✅ |
| `blurs/gizmo-composite-blur-aa-lv2-fsr-gamma-ramp.slangp` | ✅ |
| `blurs/gizmo-composite-blur-aa-lv2-fsr-gsharp-resampler.slangp` | ✅ |
| `blurs/gizmo-composite-blur-aa-lv2-fsr.slangp` | ✅ |
| `blurs/kawase-glow-bspline-4taps-fsr-gamma-ramp-tonemapping.slangp` | ✅ |
| `blurs/smartblur-bspline-4taps-fsr-gamma-ramp.slangp` | ✅ |
| `blurs/smartblur-bspline-4taps-fsr-gamma-ramp-vhs.slangp` | ✅ |
| `blurs/smartblur-bspline-4taps-fsr.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-crt-fast-bilateral-super-xbr-color-mangler-colorimetry.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-crt-fast-bilateral-super-xbr.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-crtroyale-ntsc-svideo.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-fsr-crtroyale-ntsc-svideo.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-fsr-crtroyale.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-fsr-sony-crt-megatron-hdr.slangp` | ✅ |
| `crt-beam-simulator/crt-beam-simulator-nnedi3-nns16-4x-luma-fsr-crtroyale.slangp` | ✅ |
| `crt-geom-simple.slangp` | ✅ |
| `crt-hyllian-sinc-smartblur-sgenpt.slangp` | ✅ |
| `crt-hyllian-smartblur-sgenpt.slangp` | ✅ |
| `crt-lottes-multipass-interlaced-glow.slangp` | ✅ |
| `crt-ntsc-sharp/composite-glow.slangp` | ✅ |
| `crt-ntsc-sharp/composite.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x4-glow.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x4.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x5-glow.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x5.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x6-glow.slangp` | ✅ |
| `crt-ntsc-sharp/svideo-ntsc_x6.slangp` | ✅ |
| `crt-ntsc-sharp/tate-composite-glow.slangp` | ✅ |
| `crt-ntsc-sharp/tate-composite.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x4-glow.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x4.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x5-glow.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x5.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x6-glow.slangp` | ✅ |
| `crt-ntsc-sharp/tate-svideo-ntsc_x6.slangp` | ✅ |
| `crt-plus-signal/c64-monitor.slangp` | ✅ |
| `crt-plus-signal/crt-beans-s-video.slangp` | ✅ |
| `crt-plus-signal/crt-geom-deluxe-ntsc-adaptive.slangp` | ✅ |
| `crt-plus-signal/crtglow_gauss_ntsc.slangp` | ✅ |
| `crt-plus-signal/crt-royale-fast-ntsc-composite.slangp` | ✅ |
| `crt-plus-signal/crt-royale-ntsc-composite.slangp` | ✅ |
| `crt-plus-signal/crt-royale-ntsc-svideo.slangp` | ✅ |
| `crt-plus-signal/crt-royale-pal-r57shell.slangp` | ✅ |
| `crt-plus-signal/fakelottes-ntsc-composite.slangp` | ✅ |
| `crt-plus-signal/fakelottes-ntsc-svideo.slangp` | ✅ |
| `crt-plus-signal/my_old_tv.slangp` | ✅ |
| `crt-plus-signal/ntsclut-phosphorlut.slangp` | ✅ |
| `crt-plus-signal/ntsc-phosphorlut.slangp` | ✅ |
| `crt-potato/crt-potato-colorimetry-convergence.slangp` | ✅ |
| `crt-royale-downsample.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-aperture-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-aperture-genesis.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-aperture-psx.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-aperture.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-aperture-snes.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-slotmask-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-slotmask-genesis.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-slotmask-psx.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-slotmask.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-composite-slotmask-snes.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-ntsc-rf-slotmask-nes.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-rgb-aperture.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-fast-rgb-slot.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-ntsc-composite-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-ntsc-composite-genesis.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-ntsc-composite-psx.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-ntsc-composite.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-ntsc-composite-snes.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-rgb-blend.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-rgb-shmup.slangp` | ✅ |
| `crt-royale-fast/4k/crt-royale-pvm-rgb.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-aperture-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-aperture-genesis.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-aperture-psx.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-aperture.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-aperture-snes.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-slotmask-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-slotmask-genesis.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-slotmask-psx.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-slotmask.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-composite-slotmask-snes.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-ntsc-rf-slotmask-nes.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-rgb-aperture.slangp` | ✅ |
| `crt-royale-fast/crt-royale-fast-rgb-slot.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-ntsc-composite-genesis-rainbow-effect.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-ntsc-composite-genesis.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-ntsc-composite-psx.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-ntsc-composite.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-ntsc-composite-snes.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-rgb-blend.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-rgb-shmup.slangp` | ✅ |
| `crt-royale-fast/crt-royale-pvm-rgb.slangp` | ✅ |
| `crt-royale-kurozumi.slangp` | ✅ |
| `crt-royale-smooth.slangp` | ✅ |
| `crt-royale-xm29plus.slangp` | ✅ |
| `crtsim-grungy.slangp` | ✅ |
| `downsample/drez-8x-fsr-aa-lv2-bspline-4taps.slangp` | ✅ |
| `fsr/fsr-aa-lv2-bspline-4taps-ntsc-colors.slangp` | ✅ |
| `fsr/fsr-aa-lv2-bspline-4taps.slangp` | ✅ |
| `fsr/fsr-aa-lv2-deblur.slangp` | ✅ |
| `fsr/fsr-aa-lv2-glass.slangp` | ✅ |
| `fsr/fsr-aa-lv2-kawase5blur-ntsc-colors-glass.slangp` | ✅ |
| `fsr/fsr-aa-lv2-kawase5blur-ntsc-colors.slangp` | ✅ |
| `fsr/fsr-aa-lv2-median3x3-glass.slangp` | ✅ |
| `fsr/fsr-aa-lv2-median3x3-ntsc-colors-glass.slangp` | ✅ |
| `fsr/fsr-aa-lv2-median3x3-ntsc-colors.slangp` | ✅ |
| `fsr/fsr-aa-lv2-median3x3.slangp` | ✅ |
| `fsr/fsr-aa-lv2-naturalvision-glass.slangp` | ✅ |
| `fsr/fsr-aa-lv2-naturalvision.slangp` | ✅ |
| `fsr/fsr-aa-lv2-naturalvision-vhs.slangp` | ✅ |
| `fsr/fsr-aa-lv2-sabr-hybrid-deposterize.slangp` | ✅ |
| `fsr/fsr-aa-lv2.slangp` | ✅ |
| `fsr/fsr-aa-lv2-vhs.slangp` | ✅ |
| `fsr/fsr-crt-potato-bvm.slangp` | ✅ |
| `fsr/fsr-crt-potato-bvm-vhs.slangp` | ✅ |
| `fsr/fsr-crt-potato-colorimetry-convergence.slangp` | ✅ |
| `fsr/fsr-crt-potato-warm-colorimetry-convergence.slangp` | ✅ |
| `fsr/fsr-crtroyale.slangp` | ✅ |
| `fsr/fsr-crtroyale-xm29plus.slangp` | ✅ |
| `fsr/fsr-lv2-aa-chromacity-glass.slangp` | ✅ |
| `fsr/fsr-smaa-colorimetry-convergence.slangp` | ✅ |
| `gameboy-advance-dot-matrix-sepia.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-curvator.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-megadrive-curvator.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-megadrive.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-n64-curvator.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-n64.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-psx-curvator.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-psx.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-snes-4k.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-snes-curvator-4k.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-snes-dark-curvator.slangp` | ✅ |
| `gizmo-crt/gizmo-crt-snes-dark.slangp` | ✅ |
| `handheld-plus-color-mod/agb001-gba-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/ags001-gba-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/bandlimit-pixel-gba-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-dslite-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-dslite-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gba-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gba-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gbc-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gbc-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gbMicro-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-gbMicro-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-nds-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-nds-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-palm-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-palm-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-psp-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-psp-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-sp101-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-sp101-color.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-vba-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/lcd-grid-v2-vba-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+gba-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+gbc-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+image-adjustment.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+nds-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2-nds-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+psp-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v2+vba-color.slangp` | ✅ |
| `handheld-plus-color-mod/retro-v3-nds-color.slangp` | ✅ |
| `handheld-plus-color-mod/sameboy-lcd-gbc-color-motionblur.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd_720p+gba-color.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd_720p+gbc-color.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd+gba-color-4k.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd+gba-color.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd+gbc-color-4k.slangp` | ✅ |
| `handheld-plus-color-mod/simpletex_lcd+gbc-color.slangp` | ✅ |
| `imgborder-royale-kurozumi.slangp` | ✅ |
| `interpolation/bspline-4taps-aa-lv2-fsr-gamma-ramp.slangp` | ✅ |
| `interpolation/bspline-4taps-aa-lv2-fsr-powervr.slangp` | ✅ |
| `interpolation/bspline-4taps-aa-lv2-fsr.slangp` | ✅ |
| `interpolation/bspline-4taps-aa-lv2.slangp` | ✅ |
| `interpolation/bspline-4taps-fsr-vhs.slangp` | ✅ |
| `interpolation/lanczos2-5taps-fsr-gamma-ramp.slangp` | ✅ |
| `interpolation/lanczos2-5taps-fsr-gamma-ramp-vhs.slangp` | ✅ |
| `interpolation/lanczos2-5taps-fsr.slangp` | ✅ |
| `interpolation/spline36-fast-fsr-gamma-ramp.slangp` | ✅ |
| `interpolation/spline36-fast-fsr-gamma-ramp-vhs.slangp` | ✅ |
| `interpolation/spline36-fast-fsr.slangp` | ✅ |
| `mdapt+fast-bilateral+super-4xbr+scanlines.slangp` | ✅ |
| `nedi-powervr-sharpen.slangp` | ✅ |
| `nes-color-decoder+colorimetry+pixellate.slangp` | ✅ |
| `nes-color-decoder+pixellate.slangp` | ✅ |
| `pixel_transparency/pixel_transparency-authentic_gbc_fast.slangp` | ✅ |
| `pixel_transparency/pixel_transparency-lcd1x.slangp` | ✅ |
| `pixel_transparency/pixel_transparency-lcd3x.slangp` | ✅ |
| `pixel_transparency/pixel_transparency-lcd-grid-v2.slangp` | ✅ |
| `pixel_transparency/pixel_transparency-zfast_lcd.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx9-aa-blur-hazy-ntsc-sh1nra358.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx9-aa-blur-hazy-vibrance-sh1nra358.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx-aa-fast.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx-aa.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx+rAA+aa-fast.slangp` | ✅ |
| `scalefx-plus-smoothing/scalefx+rAA+aa.slangp` | ✅ |
| `scalefx-plus-smoothing/xsoft+scalefx-level2aa+sharpsmoother.slangp` | ✅ |
| `scalefx-plus-smoothing/xsoft+scalefx-level2aa.slangp` | ✅ |
| `tvout+interlacing/tvout+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout-jinc-sharpen+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+nes-color-decoder+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-256px-composite+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-256px-svideo+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-2phase-composite+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-2phase-svideo+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-320px-composite+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-320px-svideo+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-3phase-composite+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-3phase-svideo+interlacing.slangp` | ✅ |
| `tvout+interlacing/tvout+ntsc-nes+interlacing.slangp` | ✅ |
| `tvout/tvout-jinc-sharpen.slangp` | ✅ |
| `tvout/tvout+nes-color-decoder.slangp` | ✅ |
| `tvout/tvout+ntsc-256px-composite.slangp` | ✅ |
| `tvout/tvout+ntsc-256px-svideo.slangp` | ✅ |
| `tvout/tvout+ntsc-2phase-composite.slangp` | ✅ |
| `tvout/tvout+ntsc-2phase-svideo.slangp` | ✅ |
| `tvout/tvout+ntsc-320px-composite.slangp` | ✅ |
| `tvout/tvout+ntsc-320px-svideo.slangp` | ✅ |
| `tvout/tvout+ntsc-3phase-composite.slangp` | ✅ |
| `tvout/tvout+ntsc-3phase-svideo.slangp` | ✅ |
| `tvout/tvout+ntsc-nes.slangp` | ✅ |
| `tvout/tvout-pixelsharp.slangp` | ✅ |
| `tvout/tvout.slangp` | ✅ |
| `tvout/tvout+snes-hires-blend.slangp` | ✅ |
| `xbr-xsal/xbr-lv3-2xsal-lv2-aa.slangp` | ✅ |
| `xbr-xsal/xbr-lv3-2xsal-lv2-aa-soft.slangp` | ✅ |
| `xbr-xsal/xbr-lv3-aa-fast.slangp` | ✅ |
| `xbr-xsal/xbr-lv3-aa-soft-fast.slangp` | ✅ |

</details>

<details>
<summary><strong>reshade</strong> (55/55)</summary>

| Preset | Status |
|--------|--------|
| `blendoverlay.slangp` | ✅ |
| `bloom.slangp` | ✅ |
| `bsnes-gamma-ramp.slangp` | ✅ |
| `FilmGrain.slangp` | ✅ |
| `halftone-print.slangp` | ✅ |
| `handheld-color-LUTs/DSLite-2020.slangp` | ✅ |
| `handheld-color-LUTs/DSLite-P3.slangp` | ✅ |
| `handheld-color-LUTs/DSLite-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/GBA-2020.slangp` | ✅ |
| `handheld-color-LUTs/GBA_GBC-2020.slangp` | ✅ |
| `handheld-color-LUTs/GBA_GBC-P3.slangp` | ✅ |
| `handheld-color-LUTs/GBA_GBC-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/GBA-P3-dark.slangp` | ✅ |
| `handheld-color-LUTs/GBA-P3.slangp` | ✅ |
| `handheld-color-LUTs/GBA-rec2020-dark.slangp` | ✅ |
| `handheld-color-LUTs/GBA-rec2020.slangp` | ✅ |
| `handheld-color-LUTs/GBA-sRGB-dark.slangp` | ✅ |
| `handheld-color-LUTs/GBA-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/GBC-2020.slangp` | ✅ |
| `handheld-color-LUTs/GBC Dev Colorspace.slangp` | ✅ |
| `handheld-color-LUTs/GBC-P3.slangp` | ✅ |
| `handheld-color-LUTs/GBC-rec2020.slangp` | ✅ |
| `handheld-color-LUTs/GBC-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/GBMicro-2020.slangp` | ✅ |
| `handheld-color-LUTs/GBMicro-P3.slangp` | ✅ |
| `handheld-color-LUTs/GBMicro-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/NDS-2020.slangp` | ✅ |
| `handheld-color-LUTs/NDS-P3.slangp` | ✅ |
| `handheld-color-LUTs/NDS-rec2020.slangp` | ✅ |
| `handheld-color-LUTs/NDS-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/NSO-GBA.slangp` | ✅ |
| `handheld-color-LUTs/NSO-GBC.slangp` | ✅ |
| `handheld-color-LUTs/PSP-2020.slangp` | ✅ |
| `handheld-color-LUTs/PSP-P3(pure-gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/PSP-P3.slangp` | ✅ |
| `handheld-color-LUTs/PSP-rec2020.slangp` | ✅ |
| `handheld-color-LUTs/PSP-rec2020(sRGB-gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/PSP-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/PSP-sRGB(sRGB-gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/SP101-2020.slangp` | ✅ |
| `handheld-color-LUTs/SP101-P3.slangp` | ✅ |
| `handheld-color-LUTs/SP101-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-P3.slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-P3(sRGB-gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-rec2020.slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-rec2020(sRGB-gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/SwitchOLED-sRGB(sRGB-Gamma2.2).slangp` | ✅ |
| `handheld-color-LUTs/SWOLED-2020.slangp` | ✅ |
| `handheld-color-LUTs/SWOLED-P3.slangp` | ✅ |
| `handheld-color-LUTs/SWOLED-sRGB.slangp` | ✅ |
| `handheld-color-LUTs/VBA Colorspace.slangp` | ✅ |
| `lut.slangp` | ✅ |
| `magicbloom.slangp` | ✅ |
| `NormalsDisplacement.slangp` | ✅ |

</details>

<details>
<summary><strong>scanlines</strong> (8/8)</summary>

| Preset | Status |
|--------|--------|
| `integer-scaling-scanlines.slangp` | ✅ |
| `ossc.slangp` | ✅ |
| `ossc_slot.slangp` | ✅ |
| `res-independent-scanlines.slangp` | ✅ |
| `scanline-fract.slangp` | ✅ |
| `scanline.slangp` | ✅ |
| `scanlines-rere.slangp` | ✅ |
| `scanlines-sine-abs.slangp` | ✅ |

</details>

<details>
<summary><strong>sharpen</strong> (6/6)</summary>

| Preset | Status |
|--------|--------|
| `adaptive-sharpen-multipass.slangp` | ✅ |
| `adaptive-sharpen.slangp` | ✅ |
| `Anime4k.slangp` | ✅ |
| `cheap-sharpen.slangp` | ✅ |
| `rca_sharpen.slangp` | ✅ |
| `super-xbr-super-res.slangp` | ✅ |

</details>

<details>
<summary><strong>stereoscopic-3d</strong> (8/8)</summary>

| Preset | Status |
|--------|--------|
| `anaglyph-to-interlaced.slangp` | ✅ |
| `anaglyph-to-side-by-side.slangp` | ✅ |
| `fubax_vr.slangp` | ✅ |
| `shutter-to-anaglyph.slangp` | ✅ |
| `shutter-to-side-by-side.slangp` | ✅ |
| `side-by-side-simple.slangp` | ✅ |
| `side-by-side-to-interlaced.slangp` | ✅ |
| `side-by-side-to-shutter.slangp` | ✅ |

</details>

<details>
<summary><strong>subframe-bfi</strong> (6/6)</summary>

| Preset | Status |
|--------|--------|
| `120hz-safe-BFI.slangp` | ✅ |
| `120hz-smart-BFI.slangp` | ✅ |
| `adaptive_strobe-koko.slangp` | ✅ |
| `bfi-simple.slangp` | ✅ |
| `crt-beam-simulator.slangp` | ✅ |
| `motionblur_test.slangp` | ✅ |

</details>

<details>
<summary><strong>vhs</strong> (7/7)</summary>

| Preset | Status |
|--------|--------|
| `gristleVHS.slangp` | ✅ |
| `mudlord-pal-vhs.slangp` | ✅ |
| `ntsc-vcr.slangp` | ✅ |
| `vhs_and_crt_godot.slangp` | ✅ |
| `vhs_mpalko.slangp` | ✅ |
| `VHSPro.slangp` | ✅ |
| `vhs.slangp` | ✅ |

</details>

<details>
<summary><strong>warp</strong> (1/1)</summary>

| Preset | Status |
|--------|--------|
| `dilation.slangp` | ✅ |

</details>

---
**Run:** `./build/debug/tests/goggles_tests "[shader][validation][batch]" -s`
