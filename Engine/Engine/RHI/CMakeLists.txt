add_library(MEngineRHI INTERFACE)

target_sources(MEngineRHI
    INTERFACE
        RHIAdapterCapabilities.h
        RHIBackend.h
        RHIBuffer.h
        RHICapabilities.h
        RHIDevice.h
        RHIDeviceCapabilities.h
        RHIFence.h
        RHIFormat.h
        RHIQueue.h
        RHIResult.h
        RHISampler.h
        RHIPipeline.h
        RHIShader.h
        RHISurface.h
        RHISwapchain.h
        RHITexture.h
        RHITextureView.h
        RHICommand.h
)

target_compile_features(MEngineRHI
    INTERFACE
        cxx_std_20
)

if(MSVC)
    # 公共 RHI 头含中文教学注释，所有读取这些头的目标均按 UTF-8 解析。
    target_compile_options(MEngineRHI INTERFACE /utf-8)
endif()

target_include_directories(MEngineRHI
    INTERFACE
        ${PROJECT_SOURCE_DIR}/Engine
)
