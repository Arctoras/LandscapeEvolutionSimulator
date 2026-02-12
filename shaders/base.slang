struct VSInput
{
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
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
    float3 normal;
    float3 colour;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.colour = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    float3 normal = normalize((2.0 * texture.Sample(vertIn.fragTexCoord).rgb) - 1.0);

    return float4(vertIn.colour * max(dot(normal, -normalize(ubo.lightDir)),0.0),1.0);
}
