## 1. Project v0 Design Snapshot (Updated)

### 1.1 High-level goal

* **Core function:** Real-time game capture + GPU post-processing.
* **Primary objectives:**

  * Minimal capture → process → display latency (gaming-grade).
  * Maximum fidelity (avoid unnecessary resampling/conversions).
* **Shader compatibility:** Post-processing should be as compatible as reasonably possible with **RetroArch shaders** (multi-pass, LUTs, parameterization), to reuse existing content and mental models.

Scope remains: focus on pipeline and feasibility, not on specific effects.

### 1.2 Target platform & ecosystem

* **OS:** Linux only in v0.
* **Display stack:**

  * Wayland as primary; X11 can be added later.
  * Avoid leaking compositor details beyond capture backends.
* **Drivers / GPU vendors:**

  * Standard Linux stack (Mesa + DRM; all major vendors via that path).
* **Language / standard:**

  * C++20 throughout.

### 1.3 Capture model

* **Primary capture path:** Vulkan capture layer

  * A Vulkan layer hooks target Vulkan applications and intercepts swapchain images.
  * Goal: zero-copy, minimal-latency access to rendered frames, similar in spirit to OBS Vulkan capture.
* **Fallback capture path:** Compositor/framebuffer capture

  * Wayland + PipeWire (and later X11) to capture the desktop/compositor output when Vulkan layer capture is not available.
* **Abstraction:** Both paths expose a common “frame source” interface (dimensions, format, timing) to the rest of the system.

### 1.4 Graphics / shader stack

* **Rendering backend:** Vulkan (primary, for both capture integration and presentation).
* **Shading stack:**

  * Slang as the main shading language, generating SPIR-V for Vulkan.
  * Shader/pipeline design should be **RetroArch-style compatible** as much as feasible:

    * Multi-pass pipeline model.
    * Support for common RetroArch semantics (textures, LUTs, uniform parameters).
    * A path to translate or reimplement RetroArch shaders in Slang or GLSL-with-Slang.
* **Architecture implication:**

  * A “pipeline” layer that models a RetroArch-like graph of passes over a source image.
  * A “render backend” layer that maps this graph onto Vulkan resources and passes.

### 1.5 Design principles (early)

* Extensible but minimal:

  * Keep clear boundaries: capture, pipeline, presentation, control.
  * Avoid over-design; implement only what is needed to support the first prototypes.
* Reversible decisions:

  * Track design decisions and assumptions; allow changes once real measurements appear.
* Focus on observability:

  * From the start, plan for basic metrics: FPS, latency estimates, backend in use.

---

### 2. Immediate Next Steps (v0 Prototyping Targets)

The early prototypes focus on **Vulkan layer capture first**. Compositor-based capture is a later fallback and is **not** part of the initial prototypes. 

#### 2.1 Prototype 1: Vulkan Layer Skeleton

* Implement a minimal Vulkan layer shared object that can be injected into a Vulkan application.
* Hook only the basics:

  * `vkCreateInstance`, `vkCreateDevice`
  * `vkCreateSwapchainKHR`
  * `vkQueuePresentKHR`
* Behavior: log or count presents, nothing more.
* Goal: verify stable hooking in real apps/games.

#### 2.2 Prototype 2: Vulkan Layer Frame Metadata

* Extend the layer to collect frame metadata:

  * Width, height, format, timing.
* Still no image sharing and no connection to the Goggles app.
* Goal: confirm the layer sees correct swapchain properties per frame.

#### 2.3 Prototype 3: Goggles Vulkan Window

* In the Goggles app, create a basic Vulkan instance/device and swapchain.
* Render a clear color or simple triangle.
* No capture, no shaders yet.
* Goal: validate our own render/present loop separately from the layer.

#### 2.4 Prototype 4: Layer → Goggles Metadata Path

* Add a minimal IPC or message path from the layer process to the Goggles app.
* Send only frame metadata (no image data) from the hooked app.
* Goal: prove multi-process communication and basic coordination.

#### 2.5 Prototype 5: First Shader Pass (RetroArch-style)

* Add a simple Slang-based shader pass in the Goggles app that runs on a dummy texture.
* Structure it as “pass 0” of a RetroArch-style pipeline (multi-pass capable, even if only one is used).
* Goal: validate the shader pipeline model before plugging in real captured frames.

---

## 3. Module Boundaries (High-Level)

To keep things maintainable and aligned with the above:

* **Capture module**

  * Backends: `vk_layer` (hook-based), `compositor` (fallback).
  * Produces abstract frame sources.

* **Pipeline module**

  * Represents a RetroArch-like pipeline (passes, textures, parameters).
  * Uses Slang shaders configured to mimic RetroArch behavior where possible.

* **Render / Vulkan module**

  * Manages Vulkan instance, device, swapchain, frame submission.
  * Executes the pipeline over the current frame and presents it.

* **App / Control module**

  * CLI or minimal UI to select capture backend, shader preset, and basic options.
