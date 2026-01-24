// Downsampling shader for pre-chain resolution control.
// Supports area filtering and Gaussian-weighted bilinear sampling.

struct PushConstants {
    float4 source_size; // [width, height, 1/width, 1/height]
    float4 target_size; // [width, height, 1/width, 1/height]
    float filter_type;  // 0 = area, 1 = gaussian
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
Sampler2D source_texture;

// Gaussian-weighted 9-tap sampling with scale-adaptive kernel.
// Sigma scales with downscale ratio to cover the correct source region.
float4 sample_gaussian(float2 texcoord, float2 texel_size) {
    float2 scale = pc.source_size.xy / pc.target_size.xy;

    // Sigma = 0.4 * scale: covers pixel footprint with less blur than 0.5
    // Lower = sharper but more aliasing, higher = softer
    float2 sigma = scale * 0.4;
    float2 offset_cardinal = sigma * texel_size;
    float2 offset_diagonal = sigma * texel_size;

    // Center sample
    float4 center = source_texture.Sample(texcoord);

    // Cardinal samples at ±sigma (4 taps)
    float4 c_px = source_texture.Sample(texcoord + float2(offset_cardinal.x, 0.0));
    float4 c_nx = source_texture.Sample(texcoord - float2(offset_cardinal.x, 0.0));
    float4 c_py = source_texture.Sample(texcoord + float2(0.0, offset_cardinal.y));
    float4 c_ny = source_texture.Sample(texcoord - float2(0.0, offset_cardinal.y));

    // Diagonal samples at (±sigma, ±sigma) (4 taps)
    float4 d_pp = source_texture.Sample(texcoord + offset_diagonal);
    float4 d_nn = source_texture.Sample(texcoord - offset_diagonal);
    float4 d_pn = source_texture.Sample(texcoord + float2(offset_diagonal.x, -offset_diagonal.y));
    float4 d_np = source_texture.Sample(texcoord + float2(-offset_diagonal.x, offset_diagonal.y));

    // Gaussian weights (precomputed, normalized to 1.0):
    // center: exp(0) = 1.0 -> 0.204
    // cardinal at sigma: exp(-0.5) = 0.606 -> 0.124 each
    // diagonal at sqrt(2)*sigma: exp(-1.0) = 0.368 -> 0.075 each
    const float w_c = 0.204;
    const float w_card = 0.124;
    const float w_diag = 0.075;

    return center * w_c
         + (c_px + c_nx + c_py + c_ny) * w_card
         + (d_pp + d_nn + d_pn + d_np) * w_diag;
}

// Area filter: weight each source pixel by its coverage overlap.
float4 sample_area(float2 texcoord) {
    float2 scale = pc.source_size.xy / pc.target_size.xy;

    float2 src_coord = texcoord * pc.source_size.xy;
    float2 box_min = src_coord - scale * 0.5;
    float2 box_max = src_coord + scale * 0.5;

    box_min = max(box_min, float2(0.0, 0.0));
    box_max = min(box_max, pc.source_size.xy);

    int2 i_min = int2(floor(box_min));
    int2 i_max = int2(ceil(box_max));

    int max_samples = 8;
    int2 sample_range = min(i_max - i_min, int2(max_samples, max_samples));
    i_max = i_min + sample_range;

    float4 accum = float4(0.0, 0.0, 0.0, 0.0);
    float total_weight = 0.0;

    for (int y = i_min.y; y < i_max.y; y++) {
        for (int x = i_min.x; x < i_max.x; x++) {
            float2 pixel_min = float2(x, y);
            float2 pixel_max = float2(x + 1, y + 1);

            float2 overlap_min = max(pixel_min, box_min);
            float2 overlap_max = min(pixel_max, box_max);
            float2 overlap_size = max(overlap_max - overlap_min, float2(0.0, 0.0));

            float weight = overlap_size.x * overlap_size.y;
            if (weight > 0.0) {
                float2 sample_uv = (float2(x, y) + 0.5) * pc.source_size.zw;
                accum += source_texture.Sample(sample_uv) * weight;
                total_weight += weight;
            }
        }
    }

    if (total_weight > 0.0) {
        return accum / total_weight;
    }

    return source_texture.Sample(texcoord);
}

[shader("pixel")] float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    float2 scale = pc.source_size.xy / pc.target_size.xy;

    if (scale.x <= 1.0 && scale.y <= 1.0) {
        return source_texture.Sample(texcoord);
    }

    if (pc.filter_type > 0.5) {
        return sample_gaussian(texcoord, pc.source_size.zw);
    }

    return sample_area(texcoord);
}
