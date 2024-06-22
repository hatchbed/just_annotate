#include <just_annotate/annotation_class_dialog.h>

#include <just_annotate/colors.h>
#include <just_annotate/imgui_util.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

AnnotationClassDialog::AnnotationClassDialog() {
    memset(name_buffer_, 0, sizeof(name_buffer_));

    SetId(0);
}

void AnnotationClassDialog::SetTitle(const std::string& title) {
    title_ = title;
}

void AnnotationClassDialog::SetId(int id) {
    next_class_id_ = id;
}

void AnnotationClassDialog::ClearExisting() {
    existing_ids_.clear();
}

void AnnotationClassDialog::AddExisting(int id) {
    existing_ids_.insert(id);
    next_class_id_ = std::max(next_class_id_, *existing_ids_.rbegin() + 1);
}

void AnnotationClassDialog::Open() {
    has_annotation_class_ = false;
    modified_annotation_class_ = false;
    deleted_annotation_class_ = false;

    annotation_class_.id = next_class_id_;
    auto& color = PLOT_COLORS[annotation_class_.id % PLOT_COLORS.size()];
    annotation_class_.color = ImVec4(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0, 1.0);

    ImGui::OpenPopup(title_.c_str());
}

void AnnotationClassDialog::Edit(const AnnotationClass& annotation_class, int index) {
    has_annotation_class_ = false;
    modified_annotation_class_ = false;
    deleted_annotation_class_ = false;

    annotation_class_ = annotation_class;
    std::fill(std::begin(name_buffer_), std::end(name_buffer_), '\0');
    size_t length_to_copy = std::min(annotation_class_.name.size(), sizeof(name_buffer_) - 1);
    annotation_class_.name.copy(name_buffer_, length_to_copy, 0);
    name_buffer_[length_to_copy] = '\0';
    original_edit_id_ = annotation_class.id;
    edit_index_ = index;
    ImGui::OpenPopup(title_.c_str());
}

void AnnotationClassDialog::Display(ImFont* icons) {
    if (ImGui::BeginPopupModal(title_.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginTable("annotation-class-table", 3)) {
            ImGui::TableNextColumn();
            ImGui::Text("Id");
            ImGui::TableNextColumn();
            ImGui::Text("Name");
            ImGui::TableNextColumn();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(50); // Set width if desired
            ImGui::DragInt("##annotation-class-id", &annotation_class_.id, 1, 0, 1000);
            ImGui::TableNextColumn();
            ImGui::PushItemWidth(100);
            ImGui::InputText("##annotation-class-name", name_buffer_, IM_ARRAYSIZE(name_buffer_));
            ImGui::TableNextColumn();
            ImGui::ColorEdit4("##annotation-class-color", (float*)&annotation_class_.color,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_NoAlpha);

            ImGui::EndTable();
        }

        if (ImGui::Button("Ok"))
        {
            if (edit_index_ < 0) {
                if (existing_ids_.count(annotation_class_.id) == 0) {
                    existing_ids_.insert(annotation_class_.id);
                    has_annotation_class_ = true;
                    next_class_id_ = *existing_ids_.rbegin() + 1;
                    annotation_class_.name = name_buffer_;
                    ImGui::CloseCurrentPopup();
                }
                else {
                    spdlog::error("Annotation class id <{}> already exists.", annotation_class_.id);
                    ImGui::OpenPopup("Error##AnnotationClassIdExists");
                }
            }
            else {
                if (original_edit_id_ == annotation_class_.id) {
                    modified_annotation_class_ = true;
                    annotation_class_.name = name_buffer_;
                    ImGui::CloseCurrentPopup();
                }
                else if (existing_ids_.count(annotation_class_.id) == 0) {
                    existing_ids_.insert(annotation_class_.id);
                    existing_ids_.erase(original_edit_id_);
                    next_class_id_ = *existing_ids_.rbegin() + 1;
                    modified_annotation_class_ = true;
                    annotation_class_.name = name_buffer_;
                    ImGui::CloseCurrentPopup();
                }
                else {
                    spdlog::error("Annotation class id <{}> already exists.", annotation_class_.id);
                    ImGui::OpenPopup("Error##AnnotationClassIdExists");
                }
            }

        }
        displayErrorMessage("Error##AnnotationClassIdExists", "Annotation class id already exists.", icons);

        if (edit_index_ >= 0) {
            ImGui::SameLine();

            if (ImGui::Button("Delete"))
            {
                // trigger confirmation window to open
                getConfirmation("Are you sure?##DeletedAnnotationClass", "Do you really want to delete this item?", icons);
            }

            // resolve opened confirmation window
            if (ImGui::IsPopupOpen("Are you sure?##DeletedAnnotationClass")) {
                int do_delete = getConfirmation("Are you sure?##DeletedAnnotationClass", "Do you really want to delete this item?", icons);
                if (do_delete == CONFIRM_YES) {
                    existing_ids_.erase(original_edit_id_);
                    if (existing_ids_.empty()) {
                        next_class_id_ = 0;
                    }
                    else {
                        next_class_id_ = *existing_ids_.rbegin() + 1;
                    }
                    deleted_annotation_class_ = true;
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            Clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

bool AnnotationClassDialog::HasAnnotationClass() const {
    return has_annotation_class_;
}

bool AnnotationClassDialog::ModifiedAnnotationClass() const {
    return modified_annotation_class_;
}

bool AnnotationClassDialog::DeletedAnnotationClass() const {
    return deleted_annotation_class_;
}

AnnotationClass AnnotationClassDialog::GetAnnotationClass() const {
    return annotation_class_;
}

int AnnotationClassDialog::GetEditIndex() const {
    return edit_index_;
}

void AnnotationClassDialog::Clear() {
    has_annotation_class_ = false;
    modified_annotation_class_ = false;
    deleted_annotation_class_ = false;
    memset(name_buffer_, 0, sizeof(name_buffer_));
    edit_index_ = -1;
    annotation_class_.name.clear();
}

