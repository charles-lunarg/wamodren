// single_triangle.slang

// Per-vertex attributes to be assembled from bound vertex buffers.
struct AssembledVertex
{
    float3 position : POSITION;
    float3 color : COLOR;
};

// Output of the vertex shader, and input to the fragment shader.
struct CoarseVertex
{
    float3 color;
};

// Output of the fragment shader
struct Fragment
{
    float4 color;
};

struct Vertex
{
    float2 pos;
    float3 color;
};

// Vertex  Shader

struct VertexStageOutput
{
    CoarseVertex coarseVertex : CoarseVertex;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexStageOutput main(uint vertexID: SV_VertexID)

{
    VertexStageOutput output;

    Vertex vertices[3] = Vertex[3](
        Vertex(float2(-0.75, 0.75), float3(1, 0, 0)),
        Vertex(float2(0.75, 0.75), float3(0, 1, 0)),
        Vertex(float2(0, -0.75), float3(0, 0, 1)));

    output.coarseVertex.color = vertices[vertexID].color;
    output.sv_position = float4(vertices[vertexID].pos, 0, 1);

    return output;
}

[shader("fragment")]
Fragment main(CoarseVertex coarseVertex: 0)
    : SV_Target
{
    float3 color = coarseVertex.color;
    Fragment output;
    output.color = float4(0, 1, 0, 1.0);
    output.color = float4(color, 1.0);
    return output;
}
