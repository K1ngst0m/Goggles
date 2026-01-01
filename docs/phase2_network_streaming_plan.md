# Phase 2 Network Streaming Plan
_Last updated: 2026-01-01_

## Objectives & Constraints
- Deliver Linux-only remote play that feels production-ready for modern games.
- Keep the Vulkan capture path + RetroArch-based Goggles viewer intact; add network transport layers without regressing shader quality.
- Operate a headless compositor service (gamescope-style) so target games believe they own a desktop session while we capture, encode, and forward their output plus input.
- Stage work so we can ship quickly with GStreamer-based components, then swap in MoQ + Vulkan Video once they are proven.

## Stream Stage Matrix
| Stage | Video | Audio | Input (Reverse) |
| --- | --- | --- | --- |
| Server Capture | Vulkan vkLayer exports DMA-BUFs out of the compositor session; compositor also surfaces frame metadata (HDR, frame pacing). | PipeWire monitor stream from headless session; per-game profiles define channel layouts. | Compositor collects relative pointer, keyboard, gamepad HID events. |
| Server Encoding | Phase 1: GStreamer `appsrc → vaapih264enc/av1enc`; Phase 2b: Vulkan Video (AV1) once driver parity confirmed. | PipeWire → GStreamer `opusenc` (5.1 ready). | Serialize via Wayland input emulation IPC; convert to lockstep packets. |
| Server Transfer | Initially RTP/SRT/QUIC-over-GStreamer for fast iteration; target Media-over-QUIC (MoQ) with congestion control hints (L4S or BBR) once stack solid. | Same channel as video (shared MoQ namespace) with clock stamps, or standalone QUIC datagrams until MoQ QoS ready. | Low-latency QUIC datagrams with FEC; optional redundant path over LAN broadcast. |
| Client Transfer | GStreamer net sources for prototype; later native MoQ subscription client embedded in Goggles viewer. | Same as video to simplify clock recovery. | QUIC datagrams arrive in viewer process, forwarded to compositor shim. |
| Client Decoding | Prototype uses GStreamer decodebin feeding dmabuf import; final path decodes using Vulkan Video surfaces owned by Goggles. | Opus decode (libopus) in viewer; feed to PipeWire sink. | Input mapper quantizes to compositor protocol and exposes per-device latency stats. |
| Client Rendering | Goggles viewer reuses existing RetroArch filter chain for upscaling/chroma reconstruction before presenting to user. | PipeWire sink handles playout drift with PTP clock sync. | Client UI shows prediction window + network health for transparency. |

## Implementation Tracks
### 1. Headless Server Runtime
- Build a minimal headless “Goggles Host” daemon that launches the target app inside a virtual display (KMS/Wayland) and injects our Vulkan layer.
- Mirror gamescope UX expectations: fake monitor EDID, frame pacing, cursor confinement, and GPU priority tuning.
- Centralize interprocess control (start/stop sessions, negotiate formats) via a gRPC or D-Bus API so transports can remain swappable.

### 2. Video Stream
- **Capture ➜ Encode:** Use existing DMA-BUF exports fed into GStreamer `appsrc` with zero-copy dmabuf import. Guard rails: add validation to ensure format modifier compatibility before pushing into encoders.
- **Encoding Strategy:** Begin with VAAPI H.264/H.265 for coverage, add AV1 when drivers stable; parallel track to prototype Vulkan Video AV1 encode path for the MoQ milestone.
- **Client Decode/Render:** Continue to terminate into the Goggles viewer; short-term rely on GStreamer decodebin feeding existing texture import, long-term own the decode using Vulkan Video so the filter chain runs without extra copies.

### 3. Audio Stream
- PipeWire monitor captures per-session mix; implement channel mapper for stereo/5.1/7.1 so codec parameters never drift from actual layout.
- Encode with `opusenc` (GStreamer) and keep timestamps aligned with video PTP clock.
- On client, decode with libopus and feed PipeWire sink with jitter buffer sized by RTT window; expose real-time sync offsets to avoid silent drift bugs.

### 4. Input Backchannel
- Capture input at the client using SDL or libinput hooks; feed into QUIC datagrams with monotonic timestamps.
- Server-side, inject events into the headless compositor via Wayland virtual input protocol; add guard to drop stale events when RTT spikes.
- Provide predictive smoothing (optional) so cursor/aim assist does not oscillate under jitter.

## Transport & Encoding Strategy
### Baseline (Q1 2026)
- Reuse GStreamer end-to-end pipelines (`appsrc → encoder → rtp{srt,quic}pay`) so we can dogfood quickly.
- Congestion control: experiment with GCC/BBR via GStreamer `webrtcdsp` or custom pacing plugin; log e2e stats into Tracy for feedback.
- Security: DTLS/SRTPS for RTP, QUIC TLS 1.3 for QUIC paths.

### Upgrade Path (Q2–Q3 2026)
- Replace GStreamer net sinks with native Media-over-QUIC stack once stabilized.
- Swap encoders to Vulkan Video so the same GPU queue handles capture + encode, cutting latency and driver variance.
- Add content-adaptive encoding (CAQ) hooks so RetroArch filter requirements influence GOP sizing and chroma subsampling automatically.

### Client Alignment
- Short-term the Goggles viewer embeds a lightweight GStreamer pipeline to receive RTP/SRT/QUIC; frames land back into the existing filter chain.
- Long-term the viewer owns the QUIC/MoQ session directly: it parses object headers, manages playout timing, and hands decoded Vulkan images straight into the filter chain without touching CPU memory.

## MoQ Feasibility (as of 2026-01-01)
- Specification status: the core MoQ draft is tracking Working Group Last Call in mid-2026; interop events in 2025 proved relay-to-relay viability but not many end-user media stacks yet.
- **Available libraries:** 
  - `moq-rs` (Rust, Mozilla/Cloudflare) – production relays but not tailored for client SDKs.
  - `libquicrq` (C, Ericsson) – low-level object framing, lacks congestion-control pluggability.
  - Chromium’s experimental WebTransport/MoQ bridge – tied to Chromium tree, heavy dependency chain.
- **C++ gap:** No turnkey C++ MoQ SDK exists today. We will need to wrap a QUIC transport (MsQuic or mvfst) and implement MoQ object/framing plus congestion hints ourselves. Estimated effort: 8–10 engineer-weeks for MVP publisher/subscriber, another 8+ weeks for production hardening (FEC, subscription renegotiation, metrics).
- **Feasibility verdict:** Achievable in ~6 months if we:
  1. Spike now on top of MsQuic (C API) with thin C++ wrappers for streams/objects.
  2. Contribute what we build upstream or maintain a fork for at least one release cycle.
  3. Keep RTP/SRT operational as a fallback until MoQ survives soak tests.
- **Key unknowns:** Lack of battle-tested congestion feedback for high-motion game workloads; how MoQ relays behave when we push multiple media/object priorities; limited QA tooling.

## Alternative / Fallback Transports
| Option | Pros | Cons | Recommendation |
| --- | --- | --- | --- |
| RTP/SRT over UDP | Mature, tooling-rich, already in Sunshine; integrates easily with GStreamer. | Higher latency floor, weaker NAT traversal without extra logic. | Use for bring-up + regression fallback, but do not invest in deep custom features. |
| WebRTC (libwebrtc) | Ecosystem maturity, TURN/STUN baked in. | Complex API, harder to keep RetroArch viewer lean; licensing friction with headless host. | Keep as escalation path for browser client experiments only. |
| QUIC datagrams + custom framing | Full control, easier to cohabit with Goggles viewer. | Reinventing reliability features; no relay ecosystem. | Only use for the input backchannel and telemetry. |

## Risk Register & Critical Flaws To Watch
1. **DMA-BUF → GStreamer zero-copy parity:** Not all drivers expose modifier metadata to GStreamer plugins; if we fall back to CPU copies the latency budget implodes. Mitigation: add capability probing and a CPU-copy fallback feature flag so we can block unsupported GPUs early.
2. **Headless compositor timing:** If gamescope-like timing is inaccurate the encoder will see jitter and hurt bitrate allocation. Need a dedicated pacing thread tied to VRR/GSYNC heuristics.
3. **A/V sync drift:** PipeWire clock recovery must stay in lockstep with video PTP; add watchdog that resamples audio or rewinds video buffer before drift exceeds 2 ms.
4. **MoQ schedule risk:** If C++ MoQ stack slips, we could stall on speculative work. Keep RTP/SRT shipping path healthy until MoQ proves stable in load tests.
5. **Input starvation during congestion:** Sharing the same QUIC connection for media and inputs can cause HOL blocking when video bursts arrive. Ensure input datagrams use separate streams with higher priority and optional redundant path.
6. **Security surface:** Headless host + virtual input is a high-value attack surface. Lock service to per-user namespaces, require mutual authentication, and audit IPC boundaries before public builds.

## Milestones
- **M1 – Streaming Prototype (end of Q1 2026):** Headless host launches game, dmabuf→GStreamer pipeline streams video+audio over RTP/SRT, client receives via embedded GStreamer and renders through existing filter chain. Input backchannel uses QUIC datagrams.
- **M2 – QUIC Upgrade (mid Q2 2026):** Replace RTP/SRT leg with QUIC transport inside GStreamer, integrate improved congestion control, add observability dashboards, and verify audio/video/input sync under packet loss.
- **M3 – MoQ Preview (end of Q3 2026):** Native C++ MoQ stack powering video/audio; Vulkan Video encode/decode path active on both ends; retro filters operate on decoded Vulkan images with no extra copies. RTP/SRT remains as gated fallback.
- **M4 – Production Harden (Q4 2026):** Complete soak tests, finalize telemetry + crash recovery, document operational runbooks for hosting multiple concurrent sessions.

---
This document forms the basis for revising `ROADMAP.md` Phase 2 tasks once we agree on the scope above.
