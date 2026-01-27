#include "imgui_backend/imgui_impl_nvn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "imgui_backend/imgui_hid_mappings.h"
#include "lib/diag/assert.hpp"
#include "program/d3/setting.hpp"
#include "program/romfs_assets.hpp"

#define UBOSIZE 0x1000
// #define PRINT(...) (static_cast<void>(sizeof(__VA_ARGS__)));
// #define PRINT_LINE(...) (static_cast<void>(sizeof(__VA_ARGS__)));

typedef float Matrix44f[4][4];

static void BuildOrthoMatrix(Matrix44f out, const ImVec2 &display_pos, const ImVec2 &display_size) {
    const float L = display_pos.x;
    const float R = display_pos.x + display_size.x;
    const float T = display_pos.y;
    const float B = display_pos.y + display_size.y;

    out[0][0] = 2.0f / (R - L);
    out[0][1] = 0.0f;
    out[0][2] = 0.0f;
    out[0][3] = 0.0f;

    out[1][0] = 0.0f;
    out[1][1] = 2.0f / (T - B);
    out[1][2] = 0.0f;
    out[1][3] = 0.0f;

    // Keep Z mapping consistent with the existing baked shader expectations.
    out[2][0] = 0.0f;
    out[2][1] = 0.0f;
    out[2][2] = -0.5f;
    out[2][3] = 0.0f;

    out[3][0] = (R + L) / (L - R);
    out[3][1] = (T + B) / (B - T);
    out[3][2] = 0.5f;
    out[3][3] = 1.0f;
}

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
            PRINT_LINE("[imgui_backend] Proof-of-life marker submitted");
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

        PRINT_LINE("[imgui_backend] Test Shader Setup.");

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
            PRINT("[imgui_backend] (Re)sized Vertex Buffer to Size: %ld", static_cast<long>(totalVtxSize));
        }

        if (!bd->vtxBuffer->IsBufferReady()) {
            PRINT_LINE("[imgui_backend] Cannot Draw Data! Buffers are not Ready.");
            return;
        }

        ImDrawVert *verts = (ImDrawVert *) bd->vtxBuffer->GetMemPtr();

        float scale = 3.0f;

        float imageX = 1 * scale;
        float imageY = 1 * scale;

        createQuad(verts, 0, (io.DisplaySize.x / 2) - (imageX), (io.DisplaySize.y / 2) - (imageY), imageX, imageY,
                   IM_COL32_WHITE);

        bd->cmdBuf->BeginRecording();
        bd->cmdBuf->BindProgram(&bd->shaderProgram, nvn::ShaderStageBits::VERTEX | nvn::ShaderStageBits::FRAGMENT);

        bd->cmdBuf->BindUniformBuffer(nvn::ShaderStage::VERTEX, 0, *bd->uniformMemory, UBOSIZE);
        Matrix44f proj_matrix{};
        BuildOrthoMatrix(proj_matrix, ImVec2(0.0f, 0.0f), io.DisplaySize);
        bd->cmdBuf->UpdateUniformBuffer(*bd->uniformMemory, UBOSIZE, 0, sizeof(proj_matrix), &proj_matrix);

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

        static std::vector<unsigned char> s_romfs_imgui_bin;
        static bool s_romfs_tried = false;
        static bool s_romfs_loaded = false;

        if (!s_romfs_tried) {
            s_romfs_tried = true;
            s_romfs_loaded = d3::romfs::ReadFileToBytes("romfs:/d3gui/imgui.bin", s_romfs_imgui_bin, 4 * 1024 * 1024);

            if (s_romfs_loaded) {
                PRINT("[imgui_backend] Using romfs imgui.bin (%lu bytes)",
                      static_cast<unsigned long>(s_romfs_imgui_bin.size()));
            } else {
                PRINT_LINE("[imgui_backend] ERROR: romfs imgui.bin not found");
            }
        }

        if (!s_romfs_loaded || s_romfs_imgui_bin.empty()) {
            PRINT_LINE("[imgui_backend] ERROR: imgui.bin is empty");
            return false;
        }

        bd->imguiShaderBinary.size = static_cast<ulong>(s_romfs_imgui_bin.size());
        bd->imguiShaderBinary.ptr = reinterpret_cast<u8*>(s_romfs_imgui_bin.data());

        return true;
    }

    bool setupShaders(u8 *shaderBinary, ulong binarySize) {

        auto bd = getBackendData();

        if (!bd->shaderProgram.Initialize(bd->device)) {
            PRINT_LINE("[imgui_backend] Failed to Initialize Shader Program!");
            return false;
        }

        bd->shaderMemory = IM_NEW(MemoryBuffer)(binarySize, nvn::MemoryPoolFlags::CPU_UNCACHED |
                                                                          nvn::MemoryPoolFlags::GPU_CACHED |
                                                                          nvn::MemoryPoolFlags::SHADER_CODE);

        if (!bd->shaderMemory->IsBufferReady()) {
            PRINT_LINE("[imgui_backend] Shader Memory Pool not Ready! Unable to continue.");
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
            PRINT_LINE("[imgui_backend] Failed to Set shader data for program.");
            return false;
        }

        bd->shaderProgram.SetDebugLabel("ImGuiShader");

        // Uniform Block _Object Memory Setup

        bd->uniformMemory = IM_NEW(MemoryBuffer)(UBOSIZE);

        if (!bd->uniformMemory->IsBufferReady()) {
            PRINT_LINE("[imgui_backend] Uniform Memory Pool not Ready! Unable to continue.");
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
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.DisplaySize = ImVec2(1280, 720); // default size

        auto *bd = IM_NEW(NvnBackendData)();
        EXL_ASSERT(bd, "Backend was not Created!");

        io.BackendRendererUserData = (void *) bd;

        bd->device = initInfo.device;
        bd->queue = initInfo.queue;
        bd->cmdBuf = initInfo.cmdBuf;
        bd->viewportSize = io.DisplaySize;
        bd->isInitialized = false;

        // Fonts are uploaded via ImGui's texture API (see TextureSupport::ProcessTextures).

        if (!createShaders()) {
            PRINT_LINE("[imgui_backend] ERROR: createShaders failed");
            return;
        }

        if (bd->isUseTestShader) {
            initTestShader();
        }

        if (!setupShaders(bd->imguiShaderBinary.ptr, bd->imguiShaderBinary.size)) {
            PRINT_LINE("[imgui_backend] ERROR: setupShaders failed");
            return;
        }

        bd->isInitialized = true;
    }

    void ShutdownBackend() {
        TextureSupport::ShutdownTextures();
        TextureSupport::ShutdownDescriptorPools();
        ImGuiIO &io = ImGui::GetIO();
        io.BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;
    }

    void updateInput() {
        // No input wired for bringup.
    }

    void newFrame() {
        ImGuiIO &io = ImGui::GetIO();
        auto bd = getBackendData();

        io.DisplaySize = bd->viewportSize;
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
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

        TextureSupport::ProcessTextures(drawData);
        if (!TextureSupport::AreDescriptorPoolsReady()) {
            return;
        }

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
                PRINT("[imgui_backend] Resizing Vertex Buffer to Size: %ld", static_cast<long>(totalVtxSize));
            } else {
                PRINT("[imgui_backend] Initializing Vertex Buffer to Size: %ld", static_cast<long>(totalVtxSize));
            }

            bd->vtxBuffer = IM_NEW(MemoryBuffer)(totalVtxSize);
        }

        // initializes/resizes buffer used for all index data created by ImGui
        size_t totalIdxSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        if (!bd->idxBuffer || bd->idxBuffer->GetPoolSize() < totalIdxSize) {
            if (bd->idxBuffer) {

                bd->idxBuffer->Finalize();
                IM_FREE(bd->idxBuffer);

                PRINT("[imgui_backend] Resizing Index Buffer to Size: %ld", static_cast<long>(totalIdxSize));
            } else {
                PRINT("[imgui_backend] Initializing Index Buffer to Size: %ld", static_cast<long>(totalIdxSize));
            }

            bd->idxBuffer = IM_NEW(MemoryBuffer)(totalIdxSize);

        }

        // if we fail to resize/init either buffers, end execution before we try to use said invalid buffer(s)
        if (!(bd->vtxBuffer->IsBufferReady() && bd->idxBuffer->IsBufferReady())) {
            PRINT_LINE("[imgui_backend] Cannot Draw Data! Buffers are not Ready.");
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
        Matrix44f proj_matrix{};
        BuildOrthoMatrix(proj_matrix, drawData->DisplayPos, drawData->DisplaySize);
        bd->cmdBuf->UpdateUniformBuffer(*bd->uniformMemory, UBOSIZE, 0, sizeof(proj_matrix),
                                        &proj_matrix); // add projection matrix data to uniform data

        setRenderStates(); // sets up the rest of the render state, required so that our shader properly gets drawn to the screen

        size_t vtxOffset = 0, idxOffset = 0;
        nvn::TextureHandle boundTextureHandle = 0;
        bool has_bound_texture = false;

        const ImVec2 clip_off = drawData->DisplayPos;
        const ImVec2 clip_scale = drawData->FramebufferScale;
        const ImVec2 vp = bd->viewportSize;
        bd->cmdBuf->SetViewport(0, 0, vp.x, vp.y);
        bd->cmdBuf->SetScissor(0, 0, vp.x, vp.y);

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

            for (const ImDrawCmd &cmd: cmdList->CmdBuffer) {
                if (cmd.UserCallback != nullptr) {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState) {
                        setRenderStates();
                    } else {
                        cmd.UserCallback(cmdList, const_cast<ImDrawCmd *>(&cmd));
                    }
                    continue;
                }

                ImVec4 clip_rect;
                clip_rect.x = (cmd.ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (cmd.ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (cmd.ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (cmd.ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x >= vp.x || clip_rect.y >= vp.y || clip_rect.z <= 0.0f || clip_rect.w <= 0.0f) {
                    continue;
                }

                const int scissor_x = std::max(0, static_cast<int>(std::floor(clip_rect.x)));
                const int scissor_y = std::max(0, static_cast<int>(std::floor(clip_rect.y)));
                const int scissor_z = std::min(static_cast<int>(vp.x), static_cast<int>(std::ceil(clip_rect.z)));
                const int scissor_w = std::min(static_cast<int>(vp.y), static_cast<int>(std::ceil(clip_rect.w)));
                const int scissor_w_px = std::max(0, scissor_z - scissor_x);
                const int scissor_h_px = std::max(0, scissor_w - scissor_y);
                if (scissor_w_px == 0 || scissor_h_px == 0) {
                    continue;
                }
                bd->cmdBuf->SetScissor(scissor_x, scissor_y, scissor_w_px, scissor_h_px);

                ImTextureID tex_id = cmd.GetTexID();
                if (tex_id == ImTextureID_Invalid) {
                    continue;
                }
                auto *handle = reinterpret_cast<nvn::TextureHandle *>(static_cast<uintptr_t>(tex_id));
                const nvn::TextureHandle tex_handle = *handle;

                if (!has_bound_texture || boundTextureHandle != tex_handle) {
                    boundTextureHandle = tex_handle;
                    has_bound_texture = true;
                    bd->cmdBuf->BindTexture(nvn::ShaderStage::FRAGMENT, 0, tex_handle);
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

        // Important: restore full-screen scissor so we don't leak a clipped scissor state into the game's next frame.
        bd->cmdBuf->SetScissor(0, 0, vp.x, vp.y);

        // end the command recording and submit to queue.
        auto handle = bd->cmdBuf->EndRecording();
        bd->queue->SubmitCommands(1, &handle);
    }
}
