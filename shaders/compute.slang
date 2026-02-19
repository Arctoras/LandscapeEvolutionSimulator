struct Dimensions {
    uint width;
    uint height;
}
ConstantBuffer<Dimensions> dim;

struct Vertex {
    float3 pos;
    float3 normal;
    float4 states;
}

struct VertexSSBO {
    Vertex vertices;
}
StructuredBuffer<VertexSSBO> verticesIn;
RWStructuredBuffer<VertexSSBO> verticesOut;

[shader("compute")]
[numthreads(64,1,1)]
void simulate(uint3 threadId : SV_DispatchThreadID)
{
    // Simulation Parameters:
    float r = 2;    // amount of rain per pixel
    float m = 0.2;  // water erosion exponent
    float e = 0.1;  // erosion speed
    float c = 1;    // creep speed
    float p = 0.1;  // landscape erosion proportion
    float u = 0;    // uplift
    float dt = 1;    // timestep

    uint index = threadId.x;
    uint2 coord = uint2(index % dim.width, index / dim.height);

    VertexSSBO here = verticesIn[coord.y * dim.width + coord.x];

    // Get neighbours and wrap correctly around (Edge vertices are duplicated, and so need to go 1 further when wrapping)
    VertexSSBO px;
    VertexSSBO nx;
    VertexSSBO py;
    VertexSSBO ny;
    if (coord.x + 1 == dim.width)  px = verticesIn[coord.y * dim.width + 1];
    else                           px = verticesIn[coord.y * dim.width + coord.x + 1];
    if (coord.x == 0)              nx = verticesIn[(coord.y + 1) * dim.width - 2];
    else                           nx = verticesIn[coord.y * dim.width + coord.x - 1];
    if (coord.y + 1 == dim.height) py = verticesIn[dim.width + coord.x];
    else                           py = verticesIn[(coord.y + 1) * dim.width + coord.x];
    if (coord.y == 0)              ny = verticesIn[(dim.height - 2) * dim.width + coord.x];
    else                           ny = verticesIn[(coord.y - 1) * dim.width + coord.x];

    float4 laplacians = px.vertices.states + nx.vertices.states + py.vertices.states + ny.vertices.states - 4 * here.vertices.states; // grid size of 1 means no division necessary
    float4 gradX = (px.vertices.states - nx.vertices.states) / 2;
    float4 gradY = (py.vertices.states - ny.vertices.states) / 2;

    float4 gradPX = px.vertices.states - here.vertices.states;
    float4 gradNX = nx.vertices.states - here.vertices.states;
    float4 gradPY = py.vertices.states - here.vertices.states;
    float4 gradNY = ny.vertices.states - here.vertices.states;

    float dh = c * laplacians.x - e * pow(here.vertices.states.y, m) * pow(length(float2(gradX.x, gradY.y)), m * 2) + u; // Change in bedrock height with respect to time
    float dw = here.vertices.states.y * (gradPX.y + gradNX.y + gradPY.y + gradNY.y);                                     // Change in water depth with respect to time

    verticesOut[index].vertices.states = here.vertices.states + dt * float4(dh, dw, 0.0, 0.0);
    verticesOut[index].vertices.pos = float3(here.vertices.pos.xy, verticesOut[index].vertices.states.x);

    // normals will always be 1 frame behind, because it would be really annoying to do otherwise, and it really doesn't matter
    verticesOut[index].vertices.normal = cross(normalize(float3(-1, 0, nx.vertices.pos.z) - float3(1, 0, px.vertices.pos.z)),
                   normalize(float3(0, -1, ny.vertices.pos.z) - float3(0, 1, py.vertices.pos.z)));
}