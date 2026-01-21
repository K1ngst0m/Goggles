// Area filter downsampling shader for pre-chain resolution control.
// Computes weighted average of source pixels covered by each output pixel.

struct PushConstants {
    float4 source_size;  // [width, height, 1/width, 1/height]
    float4 target_size;  // [width, height, 1/width, 1/height]
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
Sampler2D source_texture;

[shader("pixel")]
float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    // Calculate the scale ratio (how many source pixels per output pixel)
    float2 scale = pc.source_size.xy / pc.target_size.xy;

    // If not downsampling (scale <= 1), just sample directly
    if (scale.x <= 1.0 && scale.y <= 1.0) {
        return source_texture.Sample(texcoord);
    }

    // Calculate the source pixel region this output pixel covers
    float2 src_coord = texcoord * pc.source_size.xy;
    float2 box_min = src_coord - scale * 0.5;
    float2 box_max = src_coord + scale * 0.5;

    // Clamp to valid source coordinates
    box_min = max(box_min, float2(0.0, 0.0));
    box_max = min(box_max, pc.source_size.xy);

    // Integer bounds for iteration
    int2 i_min = int2(floor(box_min));
    int2 i_max = int2(ceil(box_max));

    // Limit sample count for performance (max 8x8 = 64 samples)
    int max_samples = 8;
    int2 sample_range = min(i_max - i_min, int2(max_samples, max_samples));
    i_max = i_min + sample_range;

    float4 accum = float4(0.0, 0.0, 0.0, 0.0);
    float total_weight = 0.0;

    // Area filter: weight each source pixel by its coverage
    for (int y = i_min.y; y < i_max.y; y++) {
        for (int x = i_min.x; x < i_max.x; x++) {
            // Calculate this pixel's contribution (overlap with the box)
            float2 pixel_min = float2(x, y);
            float2 pixel_max = float2(x + 1, y + 1);

            float2 overlap_min = max(pixel_min, box_min);
            float2 overlap_max = min(pixel_max, box_max);
            float2 overlap_size = max(overlap_max - overlap_min, float2(0.0, 0.0));

            float weight = overlap_size.x * overlap_size.y;
            if (weight > 0.0) {
                // Sample at pixel center
                float2 sample_uv = (float2(x, y) + 0.5) * pc.source_size.zw;
                accum += source_texture.Sample(sample_uv) * weight;
                total_weight += weight;
            }
        }
    }

    if (total_weight > 0.0) {
        return accum / total_weight;
    }

    // Fallback to direct sample
    return source_texture.Sample(texcoord);
}
