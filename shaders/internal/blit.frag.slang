[[vk::binding(0, 0)]]
Sampler2D source_texture;

[shader("pixel")]
float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    return source_texture.Sample(texcoord);
}
