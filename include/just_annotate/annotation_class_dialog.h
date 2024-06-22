#pragma once

#include <string>
#include <set>

#include <just_annotate/annotation_store.h>

class AnnotationClassDialog {
public:
    AnnotationClassDialog();
    ~AnnotationClassDialog() = default;

    void SetTitle(const std::string& title);
    void SetId(int id);
    void ClearExisting();
    void AddExisting(int id);
    void Open();
    void Edit(const AnnotationClass& annotation_class, int index);
    void Display(ImFont* icons = nullptr);

    bool HasAnnotationClass() const;
    bool ModifiedAnnotationClass() const;
    bool DeletedAnnotationClass() const;
    AnnotationClass GetAnnotationClass() const;
    int GetEditIndex() const;
    void Clear();

private:
    std::string title_;
    char name_buffer_[128];
    AnnotationClass annotation_class_;
    int next_class_id_ = -1;
    int edit_index_ = -1;
    int original_edit_id_ = -1;
    bool has_annotation_class_ = false;
    bool modified_annotation_class_ = false;
    bool deleted_annotation_class_ = false;

    std::set<int> existing_ids_;
};
