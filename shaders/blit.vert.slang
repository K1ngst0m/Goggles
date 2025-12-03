struct VSOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint vertex_id : SV_VertexID) {
    float2 pos = float2((vertex_id << 1) & 2, vertex_id & 2);
    
    VSOutput output;
    output.position = float4(pos * 2.0 - 1.0, 0.0, 1.0);
    output.texcoord = pos;
    return output;
}
