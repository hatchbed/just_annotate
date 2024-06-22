#pragma once

#include <algorithm>

#include <imgui.h>
#include <spdlog/spdlog.h>

bool MultiSpan(const char* label, std::vector<std::pair<float, float>>& selected_ranges,
               float cursor, float min_value, float max_value, const ImVec4 color,
               const ImVec2& size_arg = ImVec2(-1, 0))
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 size = size_arg;
    if (size.x <= 0.0f)
        size.x = ImGui::CalcItemWidth();
    if (size.y <= 0.0f)
        size.y = ImGui::GetFrameHeight();

    // Set cursor position and bounding box
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 bb_min = pos;
    ImVec2 bb_max = ImVec2(pos.x + size.x, pos.y + size.y);

    // Add the item to the window
    ImGui::InvisibleButton(label, size);

    bool value_changed = false;
    float mouse_pos = (ImGui::GetIO().MousePos.x - bb_min.x) / (bb_max.x - bb_min.x);
    float value = std::clamp(min_value + (max_value - min_value) * mouse_pos, min_value, max_value);

    // Calculate cursor pixel position and snap if within range
    float cursor_pixel_pos = (cursor - min_value) / (max_value - min_value) * size.x;
    float mouse_pixel_pos = ImGui::GetIO().MousePos.x - bb_min.x;
    if (abs(mouse_pixel_pos - cursor_pixel_pos) < 10.0f) {
        value = cursor;
    }

    static float drag_start_value = 0.0f;
    static std::vector<std::pair<float, float>> drag_start_ranges;
    static bool is_dragging = false;
    static bool is_unselecting = false;

    // Handle interaction
    if (ImGui::IsItemActive())
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            drag_start_value = value;
            drag_start_ranges = selected_ranges;
            is_dragging = true;
            is_unselecting = ImGui::GetIO().KeyShift;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float drag_end_value = value;
            if (drag_start_value > drag_end_value) {
                std::swap(drag_start_value, drag_end_value);
            }

            if (is_unselecting) {
                // Unselect range
                std::vector<std::pair<float, float>> new_ranges;
                for (const auto& range : selected_ranges) {
                    if (drag_start_value > range.second || drag_end_value < range.first) {
                        new_ranges.push_back(range);
                    }
                    else
                    {
                        if (drag_start_value > range.first) {
                            new_ranges.push_back(std::make_pair(range.first, drag_start_value));
                        }
                        if (drag_end_value < range.second) {
                            new_ranges.push_back(std::make_pair(drag_end_value, range.second));
                        }
                    }
                }
                selected_ranges = new_ranges;
            }
            else if (is_dragging) {
                // Select range
                selected_ranges.push_back(std::make_pair(drag_start_value, drag_end_value));
            }
        }
    }

    // Ensure ranges are sorted and non-overlapping
    std::sort(selected_ranges.begin(), selected_ranges.end());
    std::vector<std::pair<float, float>> merged_ranges;
    for (const auto& range : selected_ranges) {
        if (!merged_ranges.empty() && merged_ranges.back().second >= range.first) {
            merged_ranges.back().second = std::max(merged_ranges.back().second, range.second);
        }
        else {
            merged_ranges.push_back(range);
        }
    }
    selected_ranges = merged_ranges;

    value_changed = ImGui::IsItemDeactivated() && drag_start_ranges != selected_ranges;

    // Draw the slider background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(bb_min, bb_max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

    // Draw the selected ranges
    for (const auto& range : selected_ranges) {
        float start_pos = (range.first - min_value) / (max_value - min_value) * size.x;
        float end_pos = (range.second - min_value) / (max_value - min_value) * size.x;
        draw_list->AddRectFilled(ImVec2(bb_min.x + start_pos, bb_min.y),
                                 ImVec2(bb_min.x + end_pos, bb_max.y),
                                 ImGui::ColorConvertFloat4ToU32(color), style.FrameRounding);
    }

    // Draw the cursor
    float cursor_pos = (cursor - min_value) / (max_value - min_value) * size.x;
    ImVec2 cursor_bb_min = ImVec2(bb_min.x + cursor_pos, bb_min.y - 2);
    ImVec2 cursor_bb_max = ImVec2(bb_min.x + cursor_pos + 1, bb_max.y + 2);
    draw_list->AddRectFilled(cursor_bb_min, cursor_bb_max, ImGui::GetColorU32(ImGuiCol_Text));

    // Render the label text
    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 text_pos = ImVec2(bb_min.x + (size.x - text_size.x) * 0.5f, bb_min.y + (size.y - text_size.y) * 0.5f);
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label);


    return value_changed;
}
