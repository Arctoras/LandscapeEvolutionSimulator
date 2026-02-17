struct Parameters
{
    uint width;
    uint height;

    uint octaves;
    uint seed;

    float timeStep;

    bool generate;
};
ConstantBuffer<Parameters> ubo;

Texture2D inputImage;
RWTexture2D<float4> resultSimImage;
RWTexture2D<float4> resultBumpImage;

float generate(uint2 pos)
{
    float height = 0;
    float scale = 1;
    for (int i = 0; i < ubo.octaves; i++)
    {
        height += (sin(pos.x * scale / ubo.width) + sin(pos.y * scale / ubo.height)) / scale;
        scale *= 2;
    }

    return height * 10;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void simulate(uint3 GlobalInvocationID: SV_DispatchThreadID)
{
    if (ubo.generate)
    {
        float height = generate(GlobalInvocationID.xy);

        resultSimImage[GlobalInvocationID.xy] = float4(height, 0.0, 0.0, 1.0);
        resultBumpImage[GlobalInvocationID.xy] = float4(0.0, 0.0, 1.0, height);
    }
    else
    {
        float h[9]; // bedrock height
        float w[9]; // water depth
        //float r[9]; // regolith depth
        //float s[9]; // sediment transport

        int n = 0;
        for (int i = -1; i < 2; i++)
        {
            for (int j = -1; j < 2; j++)
            {
                float4 hwrs = inputImage[uint2(GlobalInvocationID.x + i, GlobalInvocationID.y + j)];

                h[n] = hwrs.r;
                w[n] = hwrs.g;
                //r[n] = hwrs.b;
                //s[n] = hwrs.a;

                n++;
            }
        }

        // Calculate differential equations

        resultSimImage[GlobalInvocationID.xy] = float4(h[4], w[4], 0.0, 1.0);

        float3 va = normalize(float3(2.0, 0.0, h[5] - h[3]));
        float3 vb = normalize(float3(0.0, 2.0, h[7] - h[1]));
        resultBumpImage[GlobalInvocationID.xy] = float4(cross(va, vb), h[4]);
    }
}