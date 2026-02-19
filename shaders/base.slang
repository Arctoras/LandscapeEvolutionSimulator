struct VSInput
{
    float3 pos;
    float3 normal;
};

struct UniformBuffer
{
    float4x4 view;
    float4x4 proj;
    int2 instanceOffset;

    float3 lightDir;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 colour;
    float3 normal;
    float height;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceId : SV_InstanceID)
{
    int xOffset = int(instanceId % 3) - 1;
    int yOffset = int(instanceId / 3) - 1;
    float3 position = float3(input.pos.x + ubo.instanceOffset.x * xOffset, input.pos.y + ubo.instanceOffset.y * yOffset, input.pos.z);

    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, float4(position, 1.0)));
    output.normal = input.normal;
    output.height = input.pos.z;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    float3 colour = float3(0.0);

    colour.r = max(0.0, min(1.0, (vertIn.height - 10) / 25.0));
    colour.g = max(0.0, min(1.0, (vertIn.height + 10) / 35.0));
    if (vertIn.height < 0) colour.b = 1 - min(1.0, vertIn.height / 40);
    if (vertIn.height > 40) colour = float3(1.0);

    if (length(ubo.lightDir) == 0)
    {
        return float4(colour, 1.0);
    }
    else
    {
        // float4 colour = texture.Sample(vertIn.UV);

        float3 normal = normalize(vertIn.normal);

        float lambertian = max(dot(ubo.lightDir, normal), 0.0);
        float specular = 0;

        if (lambertian > 0)
        {
            float3 viewDir = normalize(-vertIn.pos.xyz);

            float3 halfDir = normalize(ubo.lightDir + viewDir);
            float specAngle = max(dot(halfDir, normal), 0.0);
            specular = pow(specAngle, 16);
        }

        return float4(colour * 0.1 +          // Ambient
                      colour * lambertian +   // Diffuse
                      float3(1.0) * specular, // Specular
                      1.0);
    }
}