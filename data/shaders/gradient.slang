// hello-world.slang

[[vk::image_format("rgba8")]]
[[vk::binding(0, 0)]]
RWTexture2D<float4> image : register(u0, space0);

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId: SV_DispatchThreadID, uint3 groupThreadId: SV_GroupThreadID)
{
    int2 texelCoord = int2(threadId.xy);
    int2 size;
    image.GetDimensions(size.x, size.y);

    if (texelCoord.x < size.x && texelCoord.y < size.y)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (groupThreadId.x != 0 && groupThreadId.y != 0)
        {
            color.x = float(texelCoord.x) / (size.x);
            color.y = float(texelCoord.y) / (size.y);
        }

        image[texelCoord] = color;
    }
}
