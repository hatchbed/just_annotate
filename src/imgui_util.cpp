#include <just_annotate/imgui_util.h>

#include <GLFW/glfw3.h>
#include <hello_imgui/icons_font_awesome.h>
#include <spdlog/spdlog.h>

void centerText(const std::string& text) {
    // Get the content region available size
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();

    // Get the text size
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

    // Calculate the position to start the text so that it's centered
    float textX = (contentRegion.x - textSize.x) * 0.5f;

    // Ensure the position is not negative (if contentRegion.x < textSize.x)
    if (textX < 0.0f)
        textX = 0.0f;

    // Move the cursor to the calculated position
    ImGui::SetCursorPosX(textX);

    // Render the text
    ImGui::Text("%s", text.c_str());
}

void displayErrorMessage(const std::string& title, const std::string& message, ImFont* icons) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);
    if (width > 0 && height > 0) {
        float x = (width - 300) * 0.5f;
        float y = (height - 200) * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Appearing);
    }

    static int dyn_height = 200;
    ImGui::SetNextWindowSize(ImVec2(300, dyn_height));
    if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_NoResize)) {
        if (icons) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            ImGui::PushFont(icons);
            centerText(ICON_FA_EXCLAMATION_TRIANGLE);
            ImGui::PopFont();
        }

        ImGui::Dummy(ImVec2(300.0f, 5.0f));

        ImGui::TextWrapped(message.c_str());

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::Separator();

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImVec2 button_size(ImGui::GetFontSize() * 7.0f, 0.0f);
        if (ImGui::Button("Ok", button_size)) {
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))) {
            ImGui::CloseCurrentPopup();
        }

        dyn_height = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 0.5;

        ImGui::EndPopup();
    }
}

int getConfirmation(const std::string& title, const std::string& message, ImFont* icons) {
    int confirmed = CONFIRM_UNKNOWN;
    if (!ImGui::IsPopupOpen(title.c_str())) {
        ImGui::OpenPopup(title.c_str());
    }

    int width = 0;
    int height = 0;
    glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);
    if (width > 0 && height > 0) {
        float x = (width - 300) * 0.5f;
        float y = (height - 200) * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Appearing);
    }

    static int dyn_height = 200;
    ImGui::SetNextWindowSize(ImVec2(300, dyn_height));
    if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_NoResize)) {
        if (icons) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            ImGui::PushFont(icons);
            centerText(ICON_FA_QUESTION_CIRCLE);
            ImGui::PopFont();
        }

        ImGui::Dummy(ImVec2(300.0f, 5.0f));

        ImGui::TextWrapped(message.c_str());

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::Separator();

        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        // Confirm Button
        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            confirmed = CONFIRM_YES;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        // Cancel Button
        if (ImGui::Button("No", ImVec2(120, 0))) {
            confirmed = CONFIRM_NO;
            ImGui::CloseCurrentPopup();
        }

        dyn_height = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 0.5;

        ImGui::EndPopup();
    }

    return confirmed;
}
