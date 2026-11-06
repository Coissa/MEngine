#include "D3D12RayTracing.h"
#include "D3D12Device.h"
#include "D3D12CommandList.h"

namespace GE::D3D12
{
    namespace
    {
        ComPtr<ID3D12Resource> CreateAS(ID3D12Device& device, uint64_t bytes)
        {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap.CreationNodeMask = heap.VisibleNodeMask = 1;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = bytes;
            desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            ComPtr<ID3D12Resource> result;
            Check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                 D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                 IID_PPV_ARGS(&result)),
                  "Create acceleration structure");
            return result;
        }
        void Barrier(ID3D12GraphicsCommandList& cmd, ID3D12Resource* resource)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = resource;
            cmd.ResourceBarrier(1, &barrier);
        }
    } // namespace

    std::unique_ptr<RHI::RayTracingMesh> Device::CreateRayTracingMesh(RHI::Buffer& vertices, uint32_t vertexCount,
                                                                      uint32_t stride, RHI::Buffer& indices,
                                                                      uint32_t indexCount)
    {
        ComPtr<ID3D12Device5> device;
        Check(Native()->QueryInterface(IID_PPV_ARGS(&device)), "Get DXR device");
        D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
        geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometry.Triangles.VertexBuffer = {static_cast<Buffer&>(vertices).tracked.native->GetGPUVirtualAddress(),
                                           stride};
        geometry.Triangles.VertexCount = vertexCount;
        geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometry.Triangles.IndexBuffer = static_cast<Buffer&>(indices).tracked.native->GetGPUVirtualAddress();
        geometry.Triangles.IndexCount = indexCount;
        geometry.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};
        input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        input.NumDescs = 1;
        input.pGeometryDescs = &geometry;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&input, &info);
        auto mesh = std::make_unique<RayTracingMesh>();
        mesh->result = CreateAS(*Native(), info.ResultDataMaxSizeInBytes);
        Buffer scratch(*Native(), {size_t(info.ScratchDataSizeInBytes), RHI::Memory::GPU, true});
        GetCommandContext().ExecuteImmediate(
            [&](RHI::CommandList& commands)
            {
                commands.Transition(vertices, RHI::State::ShaderResource);
                commands.Transition(indices, RHI::State::ShaderResource);
                commands.Transition(scratch, RHI::State::Storage);
                auto& cmd = static_cast<CommandList&>(commands);
                ComPtr<ID3D12GraphicsCommandList4> dxr;
                Check(cmd.Native()->QueryInterface(IID_PPV_ARGS(&dxr)), "Get DXR command list");
                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
                build.Inputs = input;
                build.DestAccelerationStructureData = mesh->result->GetGPUVirtualAddress();
                build.ScratchAccelerationStructureData = scratch.tracked.native->GetGPUVirtualAddress();
                dxr->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
                Barrier(*cmd.Native(), mesh->result.Get());
            });
        return mesh;
    }

    std::unique_ptr<RHI::RayTracingScene> Device::CreateRayTracingScene(uint32_t capacity)
    {
        ComPtr<ID3D12Device5> device;
        Check(Native()->QueryInterface(IID_PPV_ARGS(&device)), "Get DXR device");
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};
        input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        input.NumDescs = capacity;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&input, &info);
        auto scene = std::make_unique<RayTracingScene>();
        scene->result = CreateAS(*Native(), info.ResultDataMaxSizeInBytes);
        scene->scratch = std::make_unique<Buffer>(
            *Native(), RHI::BufferDesc{size_t(info.ScratchDataSizeInBytes), RHI::Memory::GPU, true});
        scene->instances = std::make_unique<Buffer>(
            *Native(), RHI::BufferDesc{size_t(capacity) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), RHI::Memory::Upload});
        return scene;
    }

    void CommandList::BuildRayTracingScene(RHI::RayTracingScene& target,
                                           std::span<const RHI::RayTracingInstance> instances)
    {
        auto& scene = static_cast<RayTracingScene&>(target);
        for (size_t i = 0; i < instances.size(); ++i)
        {
            const auto& source = instances[i];
            D3D12_RAYTRACING_INSTANCE_DESC desc{};
            for (uint32_t row = 0; row < 3; ++row)
                for (uint32_t col = 0; col < 4; ++col)
                    desc.Transform[row][col] = source.world.At(col, row);
            desc.InstanceID = source.id;
            desc.InstanceMask = 0xff;
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
            desc.AccelerationStructure = static_cast<RayTracingMesh&>(*source.mesh).result->GetGPUVirtualAddress();
            scene.instances->Write(i * sizeof(desc), &desc, sizeof(desc));
        }
        Transition(*scene.scratch, RHI::State::Storage);
        // The same per-slot result may have been read by an earlier submission.
        Barrier(*Native(), scene.result.Get());
        ComPtr<ID3D12GraphicsCommandList4> dxr;
        Check(Native()->QueryInterface(IID_PPV_ARGS(&dxr)), "Get DXR command list");
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        build.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        build.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        build.Inputs.NumDescs = uint32_t(instances.size());
        build.Inputs.InstanceDescs = scene.instances->tracked.native->GetGPUVirtualAddress();
        build.DestAccelerationStructureData = scene.result->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData = scene.scratch->tracked.native->GetGPUVirtualAddress();
        dxr->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
        Barrier(*Native(), scene.result.Get());
    }

    void CommandList::BindRayTracingScene(uint32_t binding, RHI::RayTracingScene& target)
    {
        auto address = static_cast<RayTracingScene&>(target).result->GetGPUVirtualAddress();
        if (compute)
            native->SetComputeRootShaderResourceView(binding, address);
        else
            native->SetGraphicsRootShaderResourceView(binding, address);
    }
} // namespace GE::D3D12
