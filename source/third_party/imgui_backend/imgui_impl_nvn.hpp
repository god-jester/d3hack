#pragma once

#define IMGUI_USER_CONFIG "imgui_backend/nvn_imgui_config.h"

#include "ImguiShaderCompiler.h"
#include "imgui/imgui.h"
#include "nvn/nvn_Cpp.h"
#include "nvn/nvn_CppMethods.h"
#include "types.h"
#include "MemoryBuffer.h"



#ifdef __cplusplus

namespace ImguiNvnBackend {

    // Keep these conservative: each descriptor consumes pool memory and we rarely need many
    // ImGui textures on d3hack (usually just the font atlas).
    static constexpr int MaxTexDescriptors = 128;
    static constexpr int MaxSampDescriptors = 128;

    struct NvnBackendInitInfo {
        nvn::Device *device;
        nvn::Queue *queue;
        nvn::CommandBuffer *cmdBuf;
    };

    struct NvnBackendData {

        // general data

        nvn::Device *device;
        nvn::Queue *queue;
        nvn::CommandBuffer *cmdBuf;

        // builders

        nvn::BufferBuilder bufferBuilder;
        nvn::MemoryPoolBuilder memPoolBuilder;
        // shader data

        nvn::Program shaderProgram;

        MemoryBuffer *shaderMemory;

        // The game and Ryujinx can keep multiple frames in flight.
        // Using a small ring avoids overwriting GPU-visible buffers while the GPU is still consuming them.
        static constexpr int FramesInFlight = 3;
        int frame_index = 0;

        MemoryBuffer *uniformMemory[FramesInFlight];

        nvn::ShaderData shaderDatas[2]; // 0 - Vert 1 - Frag

        nvn::VertexStreamState streamState;
        nvn::VertexAttribState attribStates[3];

        // texture data

        nvn::TexturePool texPool;
        nvn::SamplerPool samplerPool;

        nvn::MemoryPool sampTexMemPool;

        // render data

        MemoryBuffer *vtxBuffer[FramesInFlight];
        MemoryBuffer *idxBuffer[FramesInFlight];
        ImVec2 viewportSize;

        // misc data

        s64 lastTick;
        bool isInitialized;

        bool isDisableInput = true;

        CompiledData imguiShaderBinary;

        // test shader data

        bool isUseTestShader = false;
        nvn::Program testShader;
        nvn::ShaderData testShaderDatas[2]; // 0 - Vert 1 - Frag
        MemoryBuffer *testShaderBuffer;
        CompiledData testShaderBinary;
    };

    bool createShaders();

    bool setupShaders(u8 *shaderBinary, ulong binarySize);

    void InitBackend(const NvnBackendInitInfo &initInfo);

    void ShutdownBackend();

    void updateInput();

    void newFrame();

    void setRenderStates();

    void renderDrawData(ImDrawData *drawData);

    // Gate C: fontless proof-of-life. Clears the current frame render target if set.
    void SubmitProofOfLifeClear();

    // Provide the swapchain render target for the current frame.
    // The backend does not own a window/swapchain, so the caller must set this.
    void SetRenderTarget(const nvn::Texture *colorTarget);

    NvnBackendData *getBackendData();

    namespace TextureSupport {
        void ProcessTextures(ImDrawData *draw_data);
        void ShutdownTextures();
        void ShutdownDescriptorPools();
        bool AreDescriptorPoolsReady();
        bool EnsureDescriptorPools(NvnBackendData *bd);
        bool AllocateDescriptorIds(int *out_texture_id, int *out_sampler_id);
        void AdoptDescriptorPools(int next_texture_id, int next_sampler_id);
    }  // namespace TextureSupport
}; // namespace ImguiNvnBackend

#endif
