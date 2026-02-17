struct VSInput
{
    float2 pos;
    uint2 bumpId;
    float2 UV;
};

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3 lightDir;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 UV;
    float3 normal;
};

Texture2D bumpMap;

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;

    float4 bump = bumpMap[input.bumpId];

    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.pos, bump.w, 1.0))));
    output.normal = normalize((2.0 * bump.xyz) - 1.0);
    output.UV = input.UV;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    float3 normal = normalize(vertIn.normal);
    float3 colour = texture.Sample(vertIn.UV).rgb;

    return float4(colour * max(dot(normal, -normalize(ubo.lightDir)),0.0),1.0);
}
