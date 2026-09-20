cbuffer CullingConstants : register(b0)
{
    float4 frustum_planes[6];
    uint instance_count;
};

ByteAddressBuffer scene_instances : register(t1);
RWByteAddressBuffer indirect_arguments : register(u2);
RWByteAddressBuffer visible_instance_indices : register(u3);

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    const uint instance_id = dispatch_id.x;
    if (instance_id >= instance_count)
        return;

    const float4 sphere = asfloat(scene_instances.Load4(instance_id * 16));
    [unroll]
    for (uint plane_index = 0; plane_index < 6; ++plane_index)
    {
        if (dot(frustum_planes[plane_index].xyz, sphere.xyz)
                + frustum_planes[plane_index].w < -sphere.w)
            return;
    }

    indirect_arguments.Store(instance_id * 20 + 4, 1);
    visible_instance_indices.Store(instance_id * 4, instance_id);
}
