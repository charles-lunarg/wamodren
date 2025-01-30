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

    float2 vertices[3] = float2[3](float2(-1, -1), float2(3, -1), float2(-1, 3));
    float4 position = float4(vertices[vertexID], 0, 1);
    float2 uv = 0.5 * position.xy + float2(0.5, 0.5);

    output.coarseVertex.color = float3(uv, 1);
    output.sv_position = float4(vertices[vertexID], 0, 1);

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
