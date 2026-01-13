#pragma once

#include <string>

#include "imgui/imgui.h"

namespace d3::gui2::ui {

class Window {
public:
    explicit Window(std::string title, bool enabled_default = true);
    virtual ~Window() = default;

    // Returns the value from ImGui::Begin().
    bool Render();

    bool IsEnabled() const;
    void SetEnabled(bool enabled);

    bool IsOpen() const;

    const std::string& GetTitle() const;
    void SetTitle(std::string title);

    void SetDefaultPos(ImVec2 pos, ImGuiCond cond = ImGuiCond_Once);
    void ClearDefaultPos();

    void SetDefaultSize(ImVec2 size, ImGuiCond cond = ImGuiCond_Once);
    void ClearDefaultSize();

    ImGuiWindowFlags GetFlags() const;
    void SetFlags(ImGuiWindowFlags flags);

protected:
    virtual void RenderContents() = 0;
    virtual bool* GetOpenFlag();

    virtual void BeforeBegin() {}
    virtual void AfterEnd() {}

    virtual void OnEnable() {}
    virtual void OnDisable() {}

private:
    std::string title_;
    bool enabled_ = true;
    ImGuiWindowFlags flags_ = 0;

    bool has_default_pos_ = false;
    bool has_default_size_ = false;
    ImVec2 default_pos_{};
    ImVec2 default_size_{};
    ImGuiCond default_pos_cond_ = ImGuiCond_Once;
    ImGuiCond default_size_cond_ = ImGuiCond_Once;
};

}  // namespace d3::gui2::ui
