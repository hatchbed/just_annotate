#pragma once

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <imgui.h>

struct AnnotationClass {
    int id = 0;
    std::string name;
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct Annotations {
    int id = 0;
    std::vector<std::pair<float, float>> spans;
};

struct FileAnnotations {
    using Ptr      = std::shared_ptr<FileAnnotations>;
    using ConstPtr = std::shared_ptr<const FileAnnotations>;

    std::string hash;
    std::set<std::string> names;
    std::vector<Annotations> annotations;
};

typedef std::vector<std::vector<std::pair<float, float>>> AnnotationState;
struct AnnotationHistory {
    AnnotationState current;
    std::deque<AnnotationState> undo_history;
    std::deque<AnnotationState> redo_history;

    void initialize(const AnnotationState& state);
    void update(const AnnotationState& state);
    AnnotationState undo();
    AnnotationState redo();
    void clear();
};

class AnnotationStore {
  public:
    using Ptr      = std::shared_ptr<AnnotationStore>;
    using ConstPtr = std::shared_ptr<const AnnotationStore>;

    AnnotationStore() = default;
    ~AnnotationStore() = default;

    static AnnotationStore::Ptr open(const std::string& path);
    bool save(const std::string& path);
    bool isDirty() const;
    void setDirty();

    const std::string& getPath() const;

    void setFile(const std::string& name, const std::string& hash);
    bool hasFile() const;

    bool setAnnotations(const Annotations& annotations);
    std::vector<Annotations> getAnnotations();

    bool addAnnotationClass(const AnnotationClass& annotation_class);
    bool modifyAnnotationClass(int original_id, const AnnotationClass& annotation_class);
    std::vector<AnnotationClass> getAnnotationClasses() const;

    bool deleteAnnotationClass(int id);
    std::vector<AnnotationClass> getDeletedAnnotationClasses();
    bool restoreAnnotationClass(int deleted_id, int new_id);

  private:
    std::string path_;
    std::map<int, AnnotationClass> annotation_classes_;
    std::map<std::string, FileAnnotations::Ptr> annotations_;
    FileAnnotations::Ptr current_annotations_;
    bool is_dirty_ = false;
};
