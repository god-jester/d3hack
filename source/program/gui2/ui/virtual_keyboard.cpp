#include "program/gui2/ui/virtual_keyboard.hpp"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace d3::gui2::ui::virtual_keyboard {
    namespace {
        constexpr const char *kPopupName = "On-screen keyboard##d3hack_vk";

        struct State {
            bool         want_open = false;
            bool         open      = false;
            const char  *title     = nullptr;
            std::string *target    = nullptr;
            Options      options {};

            std::string edit {};
            std::string allowed {};
        };

        State g_state {};

        static auto IsAllowedChar(char ch, const State &st) -> bool {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (uch < 0x20 || uch >= 0x7F) {
                return false;
            }
            if (ch == ' ' && !st.options.allow_space) {
                return false;
            }
            if (st.allowed.empty()) {
                return true;
            }
            return st.allowed.find(ch) != std::string::npos;
        }

        static void AppendChar(State &st, char ch) {
            if (!IsAllowedChar(ch, st)) {
                return;
            }
            if (st.options.max_len > 0 && st.edit.size() >= st.options.max_len) {
                return;
            }
            st.edit.push_back(ch);
        }

        static void RenderKeyRow(State &st, const char *keys) {
            if (keys == nullptr) {
                return;
            }

            const ImGuiStyle &style = ImGui::GetStyle();
            const float       key_w = ImGui::GetFontSize() * 2.2f;
            const float       key_h = ImGui::GetFrameHeight();

            for (const char *p = keys; *p != '\0'; ++p) {
                const char ch = *p;
                if (!IsAllowedChar(ch, st)) {
                    continue;
                }
                char label[2] = {ch, '\0'};
                ImGui::PushID(label);
                if (ImGui::Button(label, ImVec2(key_w, key_h))) {
                    AppendChar(st, ch);
                }
                ImGui::PopID();
                ImGui::SameLine(0.0f, style.ItemSpacing.x * 0.6f);
            }
            ImGui::NewLine();
        }

        static void RenderKeyRowLetters(State &st, const char *keys, bool uppercase) {
            if (keys == nullptr) {
                return;
            }
            const ImGuiStyle &style = ImGui::GetStyle();
            const float       key_w = ImGui::GetFontSize() * 2.2f;
            const float       key_h = ImGui::GetFrameHeight();

            for (const char *p = keys; *p != '\0'; ++p) {
                char ch = *p;
                if (uppercase) {
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                }
                if (!IsAllowedChar(ch, st)) {
                    continue;
                }

                char label[2] = {ch, '\0'};
                ImGui::PushID(label);
                if (ImGui::Button(label, ImVec2(key_w, key_h))) {
                    AppendChar(st, ch);
                }
                ImGui::PopID();
                ImGui::SameLine(0.0f, style.ItemSpacing.x * 0.6f);
            }
            ImGui::NewLine();
        }
    }  // namespace

    void Open(const char *title, std::string *target, Options options) {
        g_state.title     = (title != nullptr) ? title : "On-screen keyboard";
        g_state.target    = target;
        g_state.options   = options;
        g_state.want_open = true;
        g_state.open      = true;
        g_state.allowed.clear();
        if (!options.allowed_chars.empty()) {
            g_state.allowed.assign(options.allowed_chars.begin(), options.allowed_chars.end());
        }
        g_state.edit = (target != nullptr) ? *target : std::string();
    }

    auto IsOpen() -> bool {
        return g_state.open;
    }

    auto Render() -> bool {
        if (!g_state.open) {
            return false;
        }

        if (g_state.want_open) {
            ImGui::OpenPopup(kPopupName);
            g_state.want_open = false;
        }

        bool         accepted = false;
        const ImVec2 center   = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal(kPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (g_state.title != nullptr && g_state.title[0] != '\0') {
                ImGui::TextUnformatted(g_state.title);
            }

            ImGui::Separator();

            ImGui::TextUnformatted("Text:");
            ImGui::BeginChild("vk_text", ImVec2(0.0f, ImGui::GetFontSize() * 2.0f + 12.0f), ImGuiChildFlags_Borders);
            ImGui::TextWrapped("%s", g_state.edit.c_str());
            ImGui::EndChild();

            ImGui::Separator();

            static bool s_uppercase = false;
            if (ImGui::Button(s_uppercase ? "abc" : "ABC")) {
                s_uppercase = !s_uppercase;
            }
            ImGui::SameLine();

            if (ImGui::Button("Backspace")) {
                if (!g_state.edit.empty()) {
                    g_state.edit.pop_back();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                g_state.edit.clear();
            }

            ImGui::Separator();

            RenderKeyRow(g_state, "1234567890");
            RenderKeyRowLetters(g_state, "qwertyuiop", s_uppercase);
            RenderKeyRowLetters(g_state, "asdfghjkl", s_uppercase);
            RenderKeyRowLetters(g_state, "zxcvbnm", s_uppercase);

            if (g_state.options.allow_space) {
                if (ImGui::Button("Space")) {
                    AppendChar(g_state, ' ');
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("-")) {
                AppendChar(g_state, '-');
            }
            ImGui::SameLine();
            if (ImGui::Button("_")) {
                AppendChar(g_state, '_');
            }
            ImGui::SameLine();
            if (ImGui::Button(".")) {
                AppendChar(g_state, '.');
            }

            ImGui::Separator();

            const bool can_accept =
                g_state.target != nullptr && (g_state.options.max_len == 0 || g_state.edit.size() <= g_state.options.max_len);

            if (!can_accept) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("OK")) {
                if (g_state.target != nullptr) {
                    *g_state.target = g_state.edit;
                    accepted        = true;
                }
                g_state.open = false;
                ImGui::CloseCurrentPopup();
            }
            if (!can_accept) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                g_state.open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        } else {
            // Popup was closed externally.
            g_state.open = false;
        }

        return accepted;
    }

}  // namespace d3::gui2::ui::virtual_keyboard
