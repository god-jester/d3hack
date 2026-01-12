#include <cmath>
#include <cstring>
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "imgui_backend/imgui_hid_mappings.h"
#include "imgui_backend/MemoryPoolMaker.h"


#include "lib/diag/assert.hpp"
#include "program/logging.hpp"
#include "imgui_bin.h"

#include <cstdarg>

namespace {
    void DebugPrint(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

    void DebugPrint(const char *fmt, ...) {
        std::va_list vl;
        va_start(vl, fmt);
        exl::log::PrintV(fmt, vl);
        va_end(vl);
    }

    void DebugPrintTagged(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

    void DebugPrintTagged(const char *tag, const char *fmt, ...) {
        exl::log::PrintFmt("%s", tag);
        std::va_list vl;
        va_start(vl, fmt);
        exl::log::PrintV(fmt, vl);
        va_end(vl);
    }
}

#define UBOSIZE 0x1000

typedef float Matrix44f[4][4];

// orthographic matrix used for shader
static const Matrix44f projMatrix = {
        {0.001563f, 0.0f,       0.0f,  0.0f},
        {0.0f,      -0.002778f, 0.0f,  0.0f},
        {0.0f,      0.0f,       -0.5f, 0.0f},
        {-1.0f,     1.0f,       0.5f,  1.0f}
};

namespace ImguiNvnBackend {

    namespace {
        const nvn::Texture *g_frame_color_target = nullptr;
        bool g_logged_fontless_clear = false;
    }

    void SetRenderTarget(const nvn::Texture *colorTarget) {
        g_frame_color_target = colorTarget;
    }


    void SubmitProofOfLifeClear() {
        auto bd = getBackendData();
        if (!bd->isInitialized) {
            return;
        }
        if (g_frame_color_target == nullptr) {
            return;
        }

        // Non-destructive proof-of-life marker.
        // Use scissor to clear a small rectangle only.
        const ImVec2 vp = bd->viewportSize;
        const int marker_x = 24;
        const int marker_y = 24;
        const int marker_w = 96;
        const int marker_h = 96;

        bd->cmdBuf->BeginRecording();
        const nvn::Texture *colors[1] = {g_frame_color_target};
        bd->cmdBuf->SetRenderTargets(1, colors, nullptr, nullptr, nullptr);
        bd->cmdBuf->SetViewport(0, 0, vp.x, vp.y);
        bd->cmdBuf->SetScissor(marker_x, marker_y, marker_w, marker_h);

        const float clearColor[4] = {1.0f, 0.0f, 1.0f, 1.0f};
        bd->cmdBuf->ClearColor(0, clearColor, nvn::ClearColorMask::RGBA);

        // Restore full scissor.
        bd->cmdBuf->SetScissor(0, 0, vp.x, vp.y);

        auto handle = bd->cmdBuf->EndRecording();
        bd->queue->SubmitCommands(1, &handle);

        if (!g_logged_fontless_clear) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Proof-of-life marker submitted\n");
            g_logged_fontless_clear = true;
        }
    }

    // doesnt get used anymore really, as back when it was needed i had a simplified shader to test with, but now I just test with the actual imgui shader
    void initTestShader() {

        auto bd = getBackendData();
        bd->testShaderBinary = ImguiShaderCompiler::CompileShader("test");

        bd->testShaderBuffer = IM_NEW(MemoryBuffer)(bd->testShaderBinary.size, bd->testShaderBinary.ptr,
                                                    nvn::MemoryPoolFlags::CPU_UNCACHED |
                                                    nvn::MemoryPoolFlags::GPU_CACHED |
                                                    nvn::MemoryPoolFlags::SHADER_CODE);

        EXL_ASSERT(bd->testShaderBuffer->IsBufferReady(), "Shader Buffer was not ready! unable to continue.");

        BinaryHeader offsetData = BinaryHeader((u32 *) bd->testShaderBinary.ptr);

        nvn::BufferAddress addr = bd->testShaderBuffer->GetBufferAddress();

        nvn::ShaderData &vertShaderData = bd->testShaderDatas[0];
        vertShaderData.data = addr + offsetData.mVertexDataOffset;
        vertShaderData.control = bd->testShaderBinary.ptr + offsetData.mVertexControlOffset;

        nvn::ShaderData &fragShaderData = bd->testShaderDatas[1];
        fragShaderData.data = addr + offsetData.mFragmentDataOffset;
        fragShaderData.control = bd->testShaderBinary.ptr + offsetData.mFragmentControlOffset;

        EXL_ASSERT(bd->testShader.Initialize(bd->device), "Unable to Init Program!");
        EXL_ASSERT(bd->testShader.SetShaders(2, bd->testShaderDatas), "Unable to Set Shaders!");

        DebugPrint("[imgui_backend] Test Shader Setup.\n");

    }

    // NOTE: Input/debug helpers from the reference repo were removed for d3hack bringup.

    // places ImDrawVerts, starting at startIndex, that use the x,y,width, and height values to define vertex coords
    void createQuad(ImDrawVert *verts, int startIndex, int x, int y, int width, int height, ImU32 col) {

        float minXVal = x;
        float maxXVal = x + width;
        float minYVal = y; // 400
        float maxYVal = y + height; // 400

        // top left
        ImDrawVert p1 = {
                .pos = ImVec2(minXVal, minYVal),
                .uv = ImVec2(0.0f, 0.0f),
                .col = col
        };
        // top right
        ImDrawVert p2 = {
                .pos = ImVec2(minXVal, maxYVal),
                .uv = ImVec2(0.0f, 1.0f),
                .col = col
        };
        // bottom left
        ImDrawVert p3 = {
                .pos = ImVec2(maxXVal, minYVal),
                .uv = ImVec2(1.0f, 0.0f),
                .col = col
        };
        // bottom right
        ImDrawVert p4 = {
                .pos = ImVec2(maxXVal, maxYVal),
                .uv = ImVec2(1.0f, 1.0f),
                .col = col
        };

        verts[startIndex] = p4;
        verts[startIndex + 1] = p2;
        verts[startIndex + 2] = p1;

        verts[startIndex + 3] = p1;
        verts[startIndex + 4] = p3;
        verts[startIndex + 5] = p4;
    }

    // this function is mainly what I used to debug the rendering of ImGui, so code is a bit messier
    void renderTestShader(ImDrawData *drawData) {

        auto bd = getBackendData();
        auto io = ImGui::GetIO();

        constexpr int triVertCount = 3;
        constexpr int quadVertCount = triVertCount * 2;

        int quadCount = 1; // modify to reflect how many quads need to be drawn per frame

        int pointCount = quadVertCount * quadCount;

        size_t totalVtxSize = pointCount * sizeof(ImDrawVert);
        if (!bd->vtxBuffer || bd->vtxBuffer->GetPoolSize() < totalVtxSize) {
            if (bd->vtxBuffer) {
                bd->vtxBuffer->Finalize();
                IM_FREE(bd->vtxBuffer);
            }
            bd->vtxBuffer = IM_NEW(MemoryBuffer)(totalVtxSize);
            DebugPrint("[imgui_backend] (Re)sized Vertex Buffer to Size: %ld\n", static_cast<long>(totalVtxSize));
        }

        if (!bd->vtxBuffer->IsBufferReady()) {
            DebugPrint("[imgui_backend] Cannot Draw Data! Buffers are not Ready.\n");
            return;
        }

        ImDrawVert *verts = (ImDrawVert *) bd->vtxBuffer->GetMemPtr();

        float scale = 3.0f;

        float imageX = 1 * scale; // bd->fontTexture.GetWidth();
        float imageY = 1 * scale; // bd->fontTexture.GetHeight();

        createQuad(verts, 0, (io.DisplaySize.x / 2) - (imageX), (io.DisplaySize.y / 2) - (imageY), imageX, imageY,
                   IM_COL32_WHITE);

        bd->cmdBuf->BeginRecording();
        bd->cmdBuf->BindProgram(&bd->shaderProgram, nvn::ShaderStageBits::VERTEX | nvn::ShaderStageBits::FRAGMENT);

        bd->cmdBuf->BindUniformBuffer(nvn::ShaderStage::VERTEX, 0, *bd->uniformMemory, UBOSIZE);
        bd->cmdBuf->UpdateUniformBuffer(*bd->uniformMemory, UBOSIZE, 0, sizeof(projMatrix), &projMatrix);

        bd->cmdBuf->BindVertexBuffer(0, (*bd->vtxBuffer), bd->vtxBuffer->GetPoolSize());

        setRenderStates();

//        bd->cmdBuf->BindTexture(nvn::ShaderStage::FRAGMENT, 0, bd->fontTexHandle);

        bd->cmdBuf->DrawArrays(nvn::DrawPrimitive::TRIANGLES, 0, pointCount);

        auto handle = bd->cmdBuf->EndRecording();
        bd->queue->SubmitCommands(1, &handle);
    }

// backend impl

    NvnBackendData *getBackendData() {
        NvnBackendData *result = ImGui::GetCurrentContext() ? (NvnBackendData *) ImGui::GetIO().BackendRendererUserData
                                                            : nullptr;
        EXL_ASSERT(result, "Backend has not been initialized!");
        return result;
    }

    bool createShaders() {
        auto bd = getBackendData();

        bd->imguiShaderBinary.size = imgui_bin_size;
        bd->imguiShaderBinary.ptr = (u8*)&imgui_bin;

        if (bd->imguiShaderBinary.size == 0) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] ERROR: imgui.bin is empty\n");
            return false;
        }

        return true;
    }

    bool setupFont() {

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Setting up ImGui Font.\n");

        auto bd = getBackendData();

        ImGuiIO &io = ImGui::GetIO();

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: begin\n");

        // init sampler and texture pools

        int sampDescSize = 0;
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetInteger(SAMPLER_DESCRIPTOR_SIZE) before\n");
        bd->device->GetInteger(nvn::DeviceInfo::SAMPLER_DESCRIPTOR_SIZE, &sampDescSize);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetInteger(SAMPLER_DESCRIPTOR_SIZE) after => %d\n", sampDescSize);
        int texDescSize = 0;
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetInteger(TEXTURE_DESCRIPTOR_SIZE) before\n");
        bd->device->GetInteger(nvn::DeviceInfo::TEXTURE_DESCRIPTOR_SIZE, &texDescSize);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetInteger(TEXTURE_DESCRIPTOR_SIZE) after => %d\n", texDescSize);

        int sampMemPoolSize = sampDescSize * MaxSampDescriptors;
        int texMemPoolSize = texDescSize * MaxTexDescriptors;
        int totalPoolSize = ALIGN_UP(sampMemPoolSize + texMemPoolSize, 0x1000);

        DebugPrintTagged(
            "[D3Hack|exlaunch] ",
            "[imgui_backend] setupFont: descriptor sizes samp=%d tex=%d; pool sizes samp=0x%x tex=0x%x total=0x%x\n",
            sampDescSize, texDescSize, sampMemPoolSize, texMemPoolSize, totalPoolSize);

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: createPool(sampTexMemPool) before\n");
        if (!MemoryPoolMaker::createPool(&bd->sampTexMemPool, totalPoolSize)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Create Texture/Sampler Memory Pool!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: createPool(sampTexMemPool) after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: samplerPool.Initialize before\n");
        if (!bd->samplerPool.Initialize(&bd->sampTexMemPool, 0, MaxSampDescriptors)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Create Sampler Pool!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: samplerPool.Initialize after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: texPool.Initialize before\n");
        if (!bd->texPool.Initialize(&bd->sampTexMemPool, sampMemPoolSize, MaxTexDescriptors)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Create Texture Pool!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: texPool.Initialize after\n");

        // convert imgui font texels

        unsigned char *pixels;
        int width, height, pixelByteSize;
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetTexDataAsRGBA32 before\n");
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &pixelByteSize);
        DebugPrintTagged("[D3Hack|exlaunch] ",
                   "[imgui_backend] setupFont: GetTexDataAsRGBA32 after => pixels=%p %dx%d bpp=%d\n",
                   pixels, width, height, pixelByteSize);
        int texPoolSize = pixelByteSize * width * height;

        DebugPrintTagged("[D3Hack|exlaunch] ",
                   "[imgui_backend] setupFont: font mempool size=0x%x (aligned=0x%lx)\n", texPoolSize,
                   static_cast<unsigned long>(ALIGN_UP(texPoolSize, 0x1000)));

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: createPool(fontMemPool) before\n");
        if (!MemoryPoolMaker::createPool(&bd->fontMemPool, ALIGN_UP(texPoolSize, 0x1000),
                                         nvn::MemoryPoolFlags::CPU_UNCACHED | nvn::MemoryPoolFlags::GPU_CACHED)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Create Font Memory Pool!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: createPool(fontMemPool) after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: texBuilder.Set* before\n");
        bd->texBuilder.SetDefaults()
                .SetDevice(bd->device)
                .SetTarget(nvn::TextureTarget::TARGET_2D)
                .SetFormat(nvn::Format::RGBA8)
                .SetSize2D(width, height)
                .SetStorage(&bd->fontMemPool, 0);

        DebugPrintTagged("[D3Hack|exlaunch] ",
                   "[imgui_backend] setupFont: texBuilder.Set* after storageSize=0x%zx storageAlign=0x%zx\n",
                   static_cast<size_t>(bd->texBuilder.GetStorageSize()),
                   static_cast<size_t>(bd->texBuilder.GetStorageAlignment()));

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: fontTexture.Initialize before\n");
        if (!bd->fontTexture.Initialize(&bd->texBuilder)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Create Font Texture!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: fontTexture.Initialize after\n");

        // setup font texture

        nvn::CopyRegion region = {
                .xoffset = 0,
                .yoffset = 0,
                .zoffset = 0,
                .width = bd->fontTexture.GetWidth(),
                .height = bd->fontTexture.GetHeight(),
                .depth = 1
        };

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: WriteTexels before (%dx%d)\n", region.width, region.height);
        bd->fontTexture.WriteTexels(nullptr, &region, pixels);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: WriteTexels after\n");
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: FlushTexels before\n");
        bd->fontTexture.FlushTexels(nullptr, &region);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: FlushTexels after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: samplerBuilder.Set* before\n");
        bd->samplerBuilder.SetDefaults()
                .SetDevice(bd->device)
                .SetMinMagFilter(nvn::MinFilter::LINEAR, nvn::MagFilter::LINEAR)
                .SetWrapMode(nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP, nvn::WrapMode::CLAMP);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: samplerBuilder.Set* after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: fontSampler.Initialize before\n");
        if (!bd->fontSampler.Initialize(&bd->samplerBuilder)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Failed to Init Font Sampler!\n");
            return false;
        }
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: fontSampler.Initialize after\n");

        bd->textureId = 257;
        bd->samplerId = 257;

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: RegisterTexture/RegisterSampler before\n");
        bd->texPool.RegisterTexture(bd->textureId, &bd->fontTexture, nullptr);
        bd->samplerPool.RegisterSampler(bd->samplerId, &bd->fontSampler);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: RegisterTexture/RegisterSampler after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ",
                   "[imgui_backend] setupFont: GetTextureHandle before (tex=%d samp=%d)\n", bd->textureId, bd->samplerId);
        bd->fontTexHandle = bd->device->GetTextureHandle(bd->textureId, bd->samplerId);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: GetTextureHandle after\n");
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: io.Fonts->SetTexID before\n");
        io.Fonts->SetTexID(&bd->fontTexHandle);
        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: io.Fonts->SetTexID after\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] Finished.\n");

        DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] setupFont: end\n");

        return true;
    }

    bool setupShaders(u8 *shaderBinary, ulong binarySize) {

        auto bd = getBackendData();

        if (!bd->shaderProgram.Initialize(bd->device)) {
            DebugPrint("[imgui_backend] Failed to Initialize Shader Program!\n");
            return false;
        }

        bd->shaderMemory = IM_NEW(MemoryBuffer)(binarySize, nvn::MemoryPoolFlags::CPU_UNCACHED |
                                                                          nvn::MemoryPoolFlags::GPU_CACHED |
                                                                          nvn::MemoryPoolFlags::SHADER_CODE);

        if (!bd->shaderMemory->IsBufferReady()) {
            DebugPrint("[imgui_backend] Shader Memory Pool not Ready! Unable to continue.\n");
            return false;
        }

        memcpy(bd->shaderMemory->GetMemPtr(), shaderBinary, binarySize);
        BinaryHeader offsetData = BinaryHeader((u32 *) shaderBinary);

        nvn::BufferAddress addr = bd->shaderMemory->GetBufferAddress();

        nvn::ShaderData &vertShaderData = bd->shaderDatas[0];
        vertShaderData.data = addr + offsetData.mVertexDataOffset;
        vertShaderData.control = shaderBinary + offsetData.mVertexControlOffset;

        nvn::ShaderData &fragShaderData = bd->shaderDatas[1];
        fragShaderData.data = addr + offsetData.mFragmentDataOffset;
        fragShaderData.control = shaderBinary + offsetData.mFragmentControlOffset;

        if (!bd->shaderProgram.SetShaders(2, bd->shaderDatas)) {
            DebugPrint("[imgui_backend] Failed to Set shader data for program.\n");
            return false;
        }

        bd->shaderProgram.SetDebugLabel("ImGuiShader");

        // Uniform Block _Object Memory Setup

        bd->uniformMemory = IM_NEW(MemoryBuffer)(UBOSIZE);

        if (!bd->uniformMemory->IsBufferReady()) {
            DebugPrint("[imgui_backend] Uniform Memory Pool not Ready! Unable to continue.\n");
            return false;
        }

        // setup vertex attrib & stream

        bd->attribStates[0].SetDefaults().SetFormat(nvn::Format::RG32F, offsetof(ImDrawVert, pos)); // pos
        bd->attribStates[1].SetDefaults().SetFormat(nvn::Format::RG32F, offsetof(ImDrawVert, uv)); // uv
        bd->attribStates[2].SetDefaults().SetFormat(nvn::Format::RGBA8, offsetof(ImDrawVert, col)); // color

        bd->streamState.SetDefaults().SetStride(sizeof(ImDrawVert));

        return true;
    }

    void InitBackend(const NvnBackendInitInfo &initInfo) {
        ImGuiIO &io = ImGui::GetIO();
        EXL_ASSERT(!io.BackendRendererUserData, "Already Initialized Imgui Backend!");

        EXL_ASSERT(io.Fonts && "ImGuiIO::Fonts must be valid");

        io.BackendPlatformName = "Switch";
        io.BackendRendererName = "imgui_impl_nvn";
        io.IniFilename = nullptr;
        io.MouseDrawCursor = true;
        io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.DisplaySize = ImVec2(1280, 720); // default size

        auto *bd = IM_NEW(NvnBackendData)();
        EXL_ASSERT(bd, "Backend was not Created!");

        io.BackendRendererUserData = (void *) bd;

        bd->device = initInfo.device;
        bd->queue = initInfo.queue;
        bd->cmdBuf = initInfo.cmdBuf;
        bd->viewportSize = io.DisplaySize;
        bd->isInitialized = false;

        // Fonts are optional for proof-of-life; do not build/decompress them in the present hook.

        if (!createShaders()) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] ERROR: createShaders failed\n");
            return;
        }

        if (bd->isUseTestShader) {
            initTestShader();
        }

        if (!setupShaders(bd->imguiShaderBinary.ptr, bd->imguiShaderBinary.size)) {
            DebugPrintTagged("[D3Hack|exlaunch] ", "[imgui_backend] ERROR: setupShaders failed\n");
            return;
        }

        bd->isInitialized = true;
    }

    void ShutdownBackend() {

    }

    void updateInput() {
        // No input wired for bringup.
    }

    void newFrame() {
        ImGuiIO &io = ImGui::GetIO();
        (void)getBackendData();

        io.DeltaTime = 1.0f / 60.0f;

        updateInput(); // update backend inputs
    }

    void setRenderStates() {

        auto bd = getBackendData();

        nvn::PolygonState polyState;
        polyState.SetDefaults();
        polyState.SetPolygonMode(nvn::PolygonMode::FILL);
        polyState.SetCullFace(nvn::Face::NONE);
        polyState.SetFrontFace(nvn::FrontFace::CCW);
        bd->cmdBuf->BindPolygonState(&polyState);

        nvn::ColorState colorState;
        colorState.SetDefaults();
        colorState.SetLogicOp(nvn::LogicOp::COPY);
        colorState.SetAlphaTest(nvn::AlphaFunc::ALWAYS);
        for (int i = 0; i < 8; ++i) {
            colorState.SetBlendEnable(i, true);
        }
        bd->cmdBuf->BindColorState(&colorState);

        nvn::BlendState blendState;
        blendState.SetDefaults();
        blendState.SetBlendFunc(nvn::BlendFunc::SRC_ALPHA, nvn::BlendFunc::ONE_MINUS_SRC_ALPHA, nvn::BlendFunc::ONE,
                                nvn::BlendFunc::ZERO);
        blendState.SetBlendEquation(nvn::BlendEquation::ADD, nvn::BlendEquation::ADD);
        bd->cmdBuf->BindBlendState(&blendState);

        nvn::DepthStencilState depthStencil;
        depthStencil.SetDefaults();
        depthStencil.SetDepthTestEnable(0);
        depthStencil.SetDepthWriteEnable(0);
        depthStencil.SetStencilTestEnable(0);
        bd->cmdBuf->BindDepthStencilState(&depthStencil);

        bd->cmdBuf->BindVertexAttribState(3, bd->attribStates);
        bd->cmdBuf->BindVertexStreamState(1, &bd->streamState);

        bd->cmdBuf->SetTexturePool(&bd->texPool);
        bd->cmdBuf->SetSamplerPool(&bd->samplerPool);
    }

    void renderDrawData(ImDrawData *drawData) {
//        if (displayPtr != nullptr) {  // I can guarantee this isn't null
//            nn::vi::GetDisplayResolution(&width, &height, displayPtr);
//            Logger::log("Display Resolution: %d x %d\n", width, height);
//        }

        // we dont need to process any data if it isnt valid
        if (!drawData->Valid) {
//            Logger::log("Draw Data was Invalid! Skipping Render.");
            return;
        }
        // if we dont have any command lists to draw, we can stop here
        if (drawData->CmdListsCount == 0) {
//            Logger::log("Command List was Empty! Skipping Render.\n");
            return;
        }

        // get both the main backend data and IO from ImGui
        auto bd = getBackendData();
        (void)ImGui::GetIO();

        // If something went wrong during backend setup, don't try to render anything.
        if (!bd->isInitialized) {
            return;
        }

        // Gate C proof-of-life clear is driven by the present hook (once per frame)
        // to avoid depending on ImDrawData validity.

        // disable imgui rendering if we are using the test shader code
        if (bd->isUseTestShader) {
            renderTestShader(drawData);
            return;
        }

        // initializes/resizes buffer used for all vertex data created by ImGui
        size_t totalVtxSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        if (!bd->vtxBuffer || bd->vtxBuffer->GetPoolSize() < totalVtxSize) {
            if (bd->vtxBuffer) {
                bd->vtxBuffer->Finalize();
                IM_FREE(bd->vtxBuffer);
                DebugPrint("[imgui_backend] Resizing Vertex Buffer to Size: %ld\n", static_cast<long>(totalVtxSize));
            } else {
                DebugPrint("[imgui_backend] Initializing Vertex Buffer to Size: %ld\n", static_cast<long>(totalVtxSize));
            }

            bd->vtxBuffer = IM_NEW(MemoryBuffer)(totalVtxSize);
        }

        // initializes/resizes buffer used for all index data created by ImGui
        size_t totalIdxSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        if (!bd->idxBuffer || bd->idxBuffer->GetPoolSize() < totalIdxSize) {
            if (bd->idxBuffer) {

                bd->idxBuffer->Finalize();
                IM_FREE(bd->idxBuffer);

                DebugPrint("[imgui_backend] Resizing Index Buffer to Size: %ld\n", static_cast<long>(totalIdxSize));
            } else {
                DebugPrint("[imgui_backend] Initializing Index Buffer to Size: %ld\n", static_cast<long>(totalIdxSize));
            }

            bd->idxBuffer = IM_NEW(MemoryBuffer)(totalIdxSize);

        }

        // if we fail to resize/init either buffers, end execution before we try to use said invalid buffer(s)
        if (!(bd->vtxBuffer->IsBufferReady() && bd->idxBuffer->IsBufferReady())) {
            DebugPrint("[imgui_backend] Cannot Draw Data! Buffers are not Ready.\n");
            return;
        }

        bd->cmdBuf->BeginRecording(); // start recording our commands to the cmd buffer

        if (g_frame_color_target) {
            const nvn::Texture *colors[1] = {g_frame_color_target};
            bd->cmdBuf->SetRenderTargets(1, colors, nullptr, nullptr, nullptr);
        }

        bd->cmdBuf->BindProgram(&bd->shaderProgram, nvn::ShaderStageBits::VERTEX |
                                                    nvn::ShaderStageBits::FRAGMENT); // bind main imgui shader

        bd->cmdBuf->BindUniformBuffer(nvn::ShaderStage::VERTEX, 0, *bd->uniformMemory,
                                      UBOSIZE); // bind uniform block ptr
        bd->cmdBuf->UpdateUniformBuffer(*bd->uniformMemory, UBOSIZE, 0, sizeof(projMatrix),
                                        &projMatrix); // add projection matrix data to uniform data

        setRenderStates(); // sets up the rest of the render state, required so that our shader properly gets drawn to the screen

        size_t vtxOffset = 0, idxOffset = 0;
        nvn::TextureHandle boundTextureHandle = 0;

        // load data into buffers, and process draw commands
        for (int i = 0; i < drawData->CmdListsCount; i++) {

            auto cmdList = drawData->CmdLists[i];

            // calc vertex and index buffer sizes
            size_t vtxSize = cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
            size_t idxSize = cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);

            // bind vtx buffer at the current offset
            bd->cmdBuf->BindVertexBuffer(0, (*bd->vtxBuffer) + vtxOffset, vtxSize);

            // copy data from imgui command list into our gpu dedicated memory
            memcpy(bd->vtxBuffer->GetMemPtr() + vtxOffset, cmdList->VtxBuffer.Data, vtxSize);
            memcpy(bd->idxBuffer->GetMemPtr() + idxOffset, cmdList->IdxBuffer.Data, idxSize);

            for (auto cmd: cmdList->CmdBuffer) {

                // im not exactly sure this scaling is a good solution,
                // for some reason imgui clipping coords are relative to 720p instead of whatever I set for disp size.
                ImVec2 newRes = getBackendData()->viewportSize;

                bd->cmdBuf->SetViewport(0, 0, newRes.x, newRes.y);
                bd->cmdBuf->SetScissor(0, 0, newRes.x, newRes.y);

                // get texture ID from the command
                nvn::TextureHandle TexID = *(nvn::TextureHandle *) cmd.GetTexID();
                // if our previous handle is different from the current, bind the texture
                if (boundTextureHandle != TexID) {
                    boundTextureHandle = TexID;
                    bd->cmdBuf->BindTexture(nvn::ShaderStage::FRAGMENT, 0, TexID);
                }
                // draw our vertices using the indices stored in the buffer, offset by the current command index offset,
                // as well as the current offset into our buffer.
                bd->cmdBuf->DrawElementsBaseVertex(nvn::DrawPrimitive::TRIANGLES,
                                                   nvn::IndexType::UNSIGNED_SHORT, cmd.ElemCount,
                                                   (*bd->idxBuffer) + (cmd.IdxOffset * sizeof(ImDrawIdx)) + idxOffset,
                                                   cmd.VtxOffset);
            }

            vtxOffset += vtxSize;
            idxOffset += idxSize;
        }

        // end the command recording and submit to queue.
        auto handle = bd->cmdBuf->EndRecording();
        bd->queue->SubmitCommands(1, &handle);
    }
}
