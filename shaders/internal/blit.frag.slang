[[vk::binding(0, 0)]]
Sampler2D source_texture;

[shader("pixel")]
float4 main(float2 texcoord : TEXCOORD0) : SV_Target0 {
    float4 color = source_texture.Sample(texcoord);
    color.a = 1.0;
    return color;
}
