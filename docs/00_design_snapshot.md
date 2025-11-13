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

## 2. Immediate Next Steps (v0 Prototyping Targets)

These are still “prototype-level” goals, but guided by the updated model.

### 2.1 Prototype 1: Fallback capture → Vulkan window

* Use compositor-based capture (Wayland + PipeWire) to get a frame stream.
* Import frames into Vulkan and present them 1:1 in a separate window (no effects).
* Measure basic latency and stability.

### 2.2 Prototype 2: Simple RetroArch-style pass via Slang

* Add a single post-processing pass implemented in Slang, compiled to SPIR-V.
* Apply it to the captured image in the Vulkan render loop.
* Structure the code so it looks like “pass 0 of a RetroArch-like pipeline,” not a one-off effect.

### 2.3 Prototype 3: Vulkan layer skeleton

* Set up a separate Vulkan layer shared object that:

  * Hooks creation of swapchains in a target Vulkan app.
  * For now, just logs or counts presented frames.
* No frame transfer or IPC yet; just confirm hooking correctness and stability.

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
