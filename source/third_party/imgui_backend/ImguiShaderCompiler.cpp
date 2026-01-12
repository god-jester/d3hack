#include "imgui_backend/ImguiShaderCompiler.h"

CompiledData ImguiShaderCompiler::CompileShader(const char * /*shaderName*/) {
    return CompiledData {
        .ptr = nullptr,
        .size = 0,
    };
}

bool ImguiShaderCompiler::CheckIsValidVersion(nvn::Device * /*device*/) {
    return false;
}

void ImguiShaderCompiler::InitializeCompiler() {
}

