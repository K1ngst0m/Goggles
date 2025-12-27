## Context

Mega-Bezel is the most complex RetroArch shader preset family with 660 presets. Supporting it validates our shader pipeline for virtually all RetroArch shaders.

### Mega-Bezel Analysis (MBZ__3__STD__GDV)

| Aspect | Value |
|--------|-------|
| Passes | 30 |
| LUT Textures | 23 (SamplerLUT1-4, IntroImage, TubeDiffuseImage, etc.) |
| Aliases | 20+ (DerezedPass, InfoCachePass, CRTPass, etc.) |
| Scale Types | source, viewport, absolute (up to 800x600) |
| Framebuffer Formats | float_framebuffer, srgb_framebuffer |

### Key Technical Gaps

1. **#reference directive** - Mega-Bezel uses modular preset structure:
   ```
   MBZ__3__STD.slangp → #reference "Base_CRT_Presets/MBZ__3__STD__GDV.slangp"
   ```

2. **OriginalHistory** - hsm-afterglow0.slang samples previous frame for phosphor persistence

3. **frame_count_mod** - NTSC passes use `frame_count_mod = 2` for alternating scanlines

## Decisions

### Frame History Ring Buffer

Store N previous Original textures in a circular buffer.

```cpp
struct FrameHistory {
    static constexpr uint32_t MAX_HISTORY = 7;  // OriginalHistory0-6
    std::array<Texture, MAX_HISTORY> textures;
    uint32_t write_index = 0;

    void push(const Texture& original);
    Texture* get(uint32_t age);  // 0 = previous frame
};
```

Auto-detect required depth by scanning shader samplers for `OriginalHistory[N]` pattern.

### Reference Directive Parsing

Parse `#reference "path"` before INI parsing:

```cpp
Result<PresetConfig> PresetParser::load(const fs::path& path, int depth = 0) {
    if (depth > 8) return Error{"Reference depth exceeded"};

    auto content = read_file(path);
    if (content.starts_with("#reference")) {
        auto ref_path = parse_reference(content);
        return load(path.parent_path() / ref_path, depth + 1);
    }
    return parse_ini(content, path);
}
```

### frame_count_mod Per-Pass

Store in PassConfig and apply in SemanticBinder:

```cpp
struct PassConfig {
    // ... existing fields
    uint32_t frame_count_mod = 0;  // 0 = no modulo
};

// In SemanticBinder::populate_push_constants()
uint32_t frame_count = (pass.frame_count_mod > 0)
    ? (absolute_frame % pass.frame_count_mod)
    : absolute_frame;
```

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Frame history memory (7 textures) | Lazy allocation based on detected depth |
| Circular reference in presets | Depth limit of 8, path cycle detection |
| Performance with 30+ passes | Already handled by existing FilterChain |

## Open Questions

- Should we support `feedback_pass` for pass-level feedback (not just frame history)?