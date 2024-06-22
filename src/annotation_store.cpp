#include <just_annotate/annotation_store.h>

#include <fstream>
#include <iomanip>
#include <iostream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

void to_json(json& j, const ImVec4& c) {
    j = json{{"r", c.x}, {"g", c.y}, {"b", c.z}};
}

void to_json(json& j, const AnnotationClass& a) {
    j = json{{"id", a.id}, {"name", a.name}, {"color", a.color}};
}

void to_json(json& j, const Annotations& a) {
    j = json{{"id", a.id}, {"spans", a.spans}};
}

void to_json(json& j, const FileAnnotations& f) {
    j = json{
        {"hash", f.hash},
        {"names", f.names},
        {"annotations", f.annotations}
    };
}

void from_json(const json& j, ImVec4& c) {
    j.at("r").get_to(c.x);
    j.at("g").get_to(c.y);
    j.at("b").get_to(c.z);
    c.w = 1;
}

void from_json(const json& j, AnnotationClass& a) {
    j.at("id").get_to(a.id);
    j.at("name").get_to(a.name);
    j.at("color").get_to(a.color);
}

void from_json(const json& j, Annotations& a) {
    j.at("id").get_to(a.id);
    j.at("spans").get_to(a.spans);
}

void from_json(const nlohmann::json& j, FileAnnotations& f) {
    j.at("hash").get_to(f.hash);
    j.at("names").get_to(f.names);
    j.at("annotations").get_to(f.annotations);
}

void AnnotationHistory::initialize(const AnnotationState& state) {
    clear();

    current = state;
}

void AnnotationHistory::update(const AnnotationState& state) {
    redo_history.clear();
    if (!current.empty()) {
        undo_history.push_back(current);
    }

    current = state;
}

AnnotationState AnnotationHistory::undo() {
    if (undo_history.empty()) {
        return current;
    }

    if (!current.empty()) {
        redo_history.push_back(current);
    }

    current = undo_history.back();
    undo_history.pop_back();

    return current;
}

AnnotationState AnnotationHistory::redo() {
    if (redo_history.empty()) {
        return current;
    }

    if (!current.empty()) {
        undo_history.push_back(current);
    }

    current = redo_history.back();
    redo_history.pop_back();

    return current;
}

void AnnotationHistory::clear() {
    undo_history.clear();
    redo_history.clear();
    current.clear();
}

AnnotationStore::Ptr AnnotationStore::open(const std::string& path) {
    std::ifstream infile(path);
    if (infile.is_open()) {
        json j;
        infile >> j;
        infile.close();

        try {
            std::vector<AnnotationClass> classes;
            std::vector<FileAnnotations> files;
            j.at("annotation_classes").get_to(classes);
            j.at("files").get_to(files);

            auto store = std::shared_ptr<AnnotationStore>(new AnnotationStore());

            for (const auto& annotation_class: classes) {
                store->addAnnotationClass(annotation_class);
            }
            for (const auto& file_annotations: files) {
                store->annotations_[file_annotations.hash] = std::make_shared<FileAnnotations>(file_annotations);
            }

            store->is_dirty_ = false;

            return store;
        }
        catch (json::exception& e) {
            spdlog::error("JSON parse error: {}", e.what());
            return {};
        }
    }

    spdlog::error("Failed to open file: {}", path);
    return {};
}

bool AnnotationStore::save(const std::string& path) {
    spdlog::info("saving annotations to: {}", path);
    std::vector<FileAnnotations> files;
    for (const auto& annotations: annotations_) {
        spdlog::info("  getting annotations from file: {}", annotations.second->hash);
        files.push_back(*annotations.second);
    }

    std::vector<AnnotationClass> classes;
    for (const auto& annotation_class: annotation_classes_) {
        classes.push_back(annotation_class.second);
    }

    json j;
    j["annotation_classes"] = classes;
    j["files"] = files;

    std::ofstream outfile(path);
    if (outfile.is_open()) {
        outfile << std::fixed << std::setprecision(6) << std::setw(4) << j << std::endl;
        outfile.close();

        is_dirty_ = false;
        return true;
    }

    spdlog::error("Failed to save annotations to: {}", path);
    return false;
}

bool AnnotationStore::isDirty() const {
    return is_dirty_;
}

void AnnotationStore::setDirty() {
    is_dirty_ = true;
}

const std::string& AnnotationStore::getPath() const {
  return path_;
}

void AnnotationStore::setFile(const std::string& name, const std::string& hash) {
    FileAnnotations::Ptr file_annotations;

    auto annotations_it = annotations_.find(hash);
    if (annotations_it == annotations_.end()) {
        file_annotations = std::make_shared<FileAnnotations>();
        file_annotations->hash = hash;
        annotations_[hash] = file_annotations;
    }
    else {
        file_annotations = annotations_it->second;
    }

    file_annotations->names.insert(name);
    current_annotations_ = file_annotations;
}

bool AnnotationStore::hasFile() const {
    return current_annotations_ != nullptr;
}

bool AnnotationStore::setAnnotations(const Annotations& annotations) {
    if (!current_annotations_) {
        spdlog::error("No annotation file selected.");
        return false;
    }

    if (annotation_classes_.count(annotations.id) == 0) {
        spdlog::error("Annotation class not found: {}", annotations.id);
        return false;
    }

    is_dirty_ = true;

    for (auto& a: current_annotations_->annotations) {
        if (a.id == annotations.id) {
            a = annotations;
            return true;
        }
    }
    current_annotations_->annotations.push_back(annotations);
    return true;
}

std::vector<Annotations> AnnotationStore::getAnnotations() {
    if (!current_annotations_) {
        return {};
    }

    return current_annotations_->annotations;
}

bool AnnotationStore::addAnnotationClass(const AnnotationClass& annotation_class) {
    auto annotation_class_it = annotation_classes_.find(annotation_class.id);
    if (annotation_class_it != annotation_classes_.end()) {
        return false;
    }

    is_dirty_ = true;

    annotation_classes_[annotation_class.id] = annotation_class;
    return true;
}

bool AnnotationStore::modifyAnnotationClass(int original_id,
                                            const AnnotationClass& annotation_class)
{
    auto annotation_class_it = annotation_classes_.find(original_id);
    if (annotation_class_it == annotation_classes_.end()) {
        spdlog::error("Original annotation class id, {}, doesn't exist.", original_id);
        return false;
    }

    if (original_id == annotation_class.id) {
        annotation_class_it->second = annotation_class;
        return true;
    }

    annotation_class_it = annotation_classes_.find(annotation_class.id);
    if (annotation_class_it !=  annotation_classes_.end()) {
        spdlog::error("New annotation class id, {}, already in use.", annotation_class.id);
        return false;
    }

    is_dirty_ = true;

    annotation_classes_.erase(original_id);
    annotation_classes_[annotation_class.id] = annotation_class;

    for (auto& file: annotations_) {
        for (auto& annotations: file.second->annotations) {
            if (annotations.id == original_id) {
                annotations.id = annotation_class.id;
            }
        }
    }

    return true;
}

std::vector<AnnotationClass> AnnotationStore::getAnnotationClasses() const {
    std::vector<AnnotationClass> annotation_classes;
    for (const auto& annotation_class: annotation_classes_) {
        if (annotation_class.second.id >= 0) {
            annotation_classes.push_back(annotation_class.second);
        }
    }

    return annotation_classes;
}

bool AnnotationStore::deleteAnnotationClass(int id) {
    if (id < 0) {
        return false;
    }

    auto annotation_class_it = annotation_classes_.find(id);
    if (annotation_class_it == annotation_classes_.end()) {
        return false;
    }

    is_dirty_ = true;

    int new_id = -1;
    if (!annotation_classes_.empty()) {
        new_id = std::min(new_id, annotation_classes_.begin()->first - 1);
    }

    auto deleted = annotation_class_it->second;
    deleted.id = new_id;

    return modifyAnnotationClass(id, deleted);
}

std::vector<AnnotationClass> AnnotationStore::getDeletedAnnotationClasses() {
    std::vector<AnnotationClass> annotation_classes;
    for (const auto& annotation_class: annotation_classes_) {
        if (annotation_class.second.id < 0) {
            annotation_classes.push_back(annotation_class.second);
        }
    }

    return annotation_classes;
}

bool AnnotationStore::restoreAnnotationClass(int deleted_id, int new_id) {
    auto annotation_class_it = annotation_classes_.find(new_id);
    if (annotation_class_it != annotation_classes_.end()) {
        return false;
    }

    annotation_class_it = annotation_classes_.find(deleted_id);
    if (annotation_class_it == annotation_classes_.end()) {
        return false;
    }

    is_dirty_ = true;

    auto restored = annotation_class_it->second;
    restored.id = new_id;

    return modifyAnnotationClass(deleted_id, restored);
}
