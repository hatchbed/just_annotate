#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <vector>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <cmrc/cmrc.hpp>
#include <hello_imgui/icons_font_awesome.h>
#include <imfilebrowser.h>
#include <imgui.h>
#include <just_annotate/annotation_class_dialog.h>
#include <just_annotate/annotation_store.h>
#include <just_annotate/config_store.h>
#include <just_annotate/hash.h>
#include <just_annotate/imgui_util.h>
#include <just_annotate/multi_span_widget.h>
#include <just_annotate/video_file.h>
#include <spdlog/spdlog.h>

CMRC_DECLARE(just_annotate::rc);

bool try_exit = false;

void handle_signal(int signal) {
    if (try_exit) {
        exit(0);
    }

    if (signal == SIGINT) {
        try_exit = true;
    }
}

void glfw_error_callback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

void glfw_exit_callback(GLFWwindow* window) {
    try_exit = true;
    glfwSetWindowShouldClose(window, GLFW_FALSE);
}

void framebuffer_size_callback(GLFWwindow* /* window */, int width, int height) {
    // Adjust the viewport
    glViewport(0, 0, width, height);

    // Optionally, set the display size for ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
}

std::string get_file_hash(const std::string& path) {
    std::string hash = sha256(path);
    if (hash.empty()) {
        spdlog::error("Failed to get hash of video: {}", path);
        ImGui::OpenPopup("Error##Hash");
    }

    return hash;
}

int main(int argc, char* argv[]) {

    std::signal(SIGINT, handle_signal);

    auto config_state = getConfig();

    auto rc_filesystem = cmrc::just_annotate::rc::get_filesystem();
    auto fa_ttf        = rc_filesystem.open("fontawesome-webfont.ttf");
    char* fa_ttf_data  = new char[fa_ttf.size()]();
    std::memcpy(fa_ttf_data, fa_ttf.begin(), fa_ttf.size());

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(config_state.window_width, config_state.window_height, "JustAnnotate", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwSetWindowSizeLimits(window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    glfwSetWindowPos(window, config_state.window_x, config_state.window_y);
    glfwSetWindowCloseCallback(window, glfw_exit_callback);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard; // Disable Keyboard Controls

    ImFontConfig config;
    config.MergeMode            = true;
    config.FontDataOwnedByAtlas = false;
    ImWchar icons_ranges[]      = {0xf000, 0xf3ff, 0};
    io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromMemoryTTF(fa_ttf_data, fa_ttf.size(), 13.0f, &config, icons_ranges);

    ImFontConfig config2;
    config2.FontDataOwnedByAtlas = false;
    auto fontawesome_small =
      io.Fonts->AddFontFromMemoryTTF(fa_ttf_data, fa_ttf.size(), 12.0f, &config2, icons_ranges);
    auto fontawesome_medium =
      io.Fonts->AddFontFromMemoryTTF(fa_ttf_data, fa_ttf.size(), 18.0f, &config2, icons_ranges);
    auto fontawesome_large =
      io.Fonts->AddFontFromMemoryTTF(fa_ttf_data, fa_ttf.size(), 22.0f, &config2, icons_ranges);
    io.Fonts->Build();

    // ImGui::PushFont(default_font);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Set the framebuffer size callback
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    just_annotate::VideoFile::init(argc, argv);

    // create a video file browser instance
    ImGui::FileBrowser videoFileDialog;
    videoFileDialog.SetTitle("Open Video File");
    videoFileDialog.SetTypeFilters({".mp4", ".ts"});

    // create a project file browser instance
    ImGui::FileBrowser openProjectDialog;
    openProjectDialog.SetTitle("Open Project");
    openProjectDialog.SetTypeFilters({".json"});

    ImGui::FileBrowser saveProjectDialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir);
    saveProjectDialog.SetTitle("Save Project");
    saveProjectDialog.SetTypeFilters({".json"});

    AnnotationClassDialog annotationClassDialog;

    // Our state
    std::string filepath;
    std::string project_path;
    std::string open_recent_path;
    std::string open_recent_video;
    std::string last_title;
    auto project = std::make_shared<AnnotationStore>();
    bool use_dark_theme = true;
    just_annotate::VideoFile::Ptr video_file;
    bool add_annotation_class = false;
    bool new_project = false;
    int edit_annotation_class = -1;
    float seek_position     = 0.0;
    bool is_seeking = false;
    bool pause_for_seeking = false;
    float video_position    = 0.0;
    std::vector<AnnotationClass> annotation_classes;
    AnnotationState annotations;
    AnnotationHistory annotation_history;
    auto last_config_save = std::chrono::high_resolution_clock::now();
    ConfigState last_config_saved;

    if (use_dark_theme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    float last_image_height = 0;
    ImVec2 remaining_space(0, 0);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        static ImGuiWindowFlags flags =
          ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
          ImGuiWindowFlags_NoNavFocus;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowBgAlpha(1.0f);

        // create window
        ImGui::Begin("", nullptr, flags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {


                if (ImGui::MenuItem("New Project")) {
                    new_project = true;
                }

                if (ImGui::MenuItem("Open Project...")) {
                    openProjectDialog.Open();
                }

                ImGui::BeginDisabled(config_state.recent_files.empty());
                if (ImGui::BeginMenu("Recent Projects")) {

                    for (const auto& recent_file: config_state.recent_files) {
                        if (ImGui::MenuItem(recent_file.c_str())) {
                            open_recent_path = recent_file;
                        }
                    }

                    ImGui::EndMenu();
                }
                ImGui::EndDisabled();

                ImGui::BeginDisabled(project_path.empty());
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) {

                    for (size_t i = 0; i < annotation_classes.size(); i++) {
                        if(!project->setAnnotations({annotation_classes[i].id, annotations[i]})) {
                            spdlog::error("Failed to set annotations.");
                        }
                    }

                    if(!project->save(project_path)) {
                        ImGui::OpenPopup("Error##SaveProject");
                    }
                }
                ImGui::EndDisabled();

                if (ImGui::MenuItem("Save As...")) {
                    saveProjectDialog.Open();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Open Video...", "Ctrl+O")) {
                    videoFileDialog.Open();
                }

                ImGui::BeginDisabled(config_state.recent_videos.empty());
                if (ImGui::BeginMenu("Recent Videos")) {

                    for (const auto& recent_video: config_state.recent_videos) {
                        if (ImGui::MenuItem(recent_video.c_str())) {
                            open_recent_video = recent_video;
                        }
                    }

                    ImGui::EndMenu();
                }
                ImGui::EndDisabled();

                ImGui::Separator();

                if (ImGui::MenuItem("Exit", "Ctrl+X")) {
                    try_exit = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Annotations")) {
                if (ImGui::MenuItem("Add Class", "")) {
                    add_annotation_class = true;
                }

                if (!annotation_classes.empty()) {
                    ImGui::Separator();
                }

                for (size_t i = 0; i < annotation_classes.size(); i++) {
                    const auto& annotation_class = annotation_classes[i];

                    std::string label = "Edit Class<";
                    label += std::to_string(annotation_class.id) + ">";
                    if (!annotation_class.name.empty()) {
                        label += ": " + annotation_class.name;
                    }

                    int icon_dim = ImGui::GetFrameHeight() - 7;
                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x, p_min.y + icon_dim);
                    p_min.x = p_max.x - icon_dim;
                    ImGui::PushFont(fontawesome_small);
                    ImGui::GetWindowDrawList()->AddText(p_min, ImGui::ColorConvertFloat4ToU32(annotation_class.color), ICON_FA_CIRCLE);
                    ImGui::PopFont();
                    if (ImGui::MenuItem(label.c_str(), "")) {
                        edit_annotation_class = static_cast<int>(i);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Preferences")) {
                if (use_dark_theme) {
                    if (ImGui::MenuItem("Style: Light")) {
                        ImGui::StyleColorsLight();
                        use_dark_theme = false;
                    }
                } else {
                    if (ImGui::MenuItem("Style: Dark")) {
                        ImGui::StyleColorsDark();
                        use_dark_theme = true;
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About JustAnnotate")) {
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (!ImGui::IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup)) {
            // check if save shortcut key was pressed
            if (!project_path.empty()) {
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_S))) {
                    for (size_t i = 0; i < annotation_classes.size(); i++) {
                        if(!project->setAnnotations({annotation_classes[i].id, annotations[i]})) {
                            spdlog::error("Failed to set annotations.");
                        }
                    }

                    if(!project->save(project_path)) {
                        ImGui::OpenPopup("Error##SaveProject");
                    }
                }
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_O))) {
                videoFileDialog.Open();
            }

            // check if exit shortcut key was pressed
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X))) {
                try_exit = true;
            }

            if (video_file && !annotation_classes.empty()) {
                if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z))) {
                    auto result = annotation_history.undo();
                    if (result.size() != annotation_classes.size()) {
                        result.clear();
                        result.resize(annotation_classes.size());
                    }
                    if (result != annotations) {
                        annotations = result;
                        project->setDirty();
                    }
                }

                if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Y))) ||
                    (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))) {
                    auto result = annotation_history.redo();
                    if (result.size() != annotation_classes.size()) {
                        result.clear();
                        result.resize(annotation_classes.size());
                    }
                    if (result != annotations) {
                        annotations = result;
                        project->setDirty();
                    }
                }
            }
        }

        if (!video_file) {
            std::string open_label = "[ Open Vide File ]";
            auto windowSize        = ImGui::GetWindowSize();
            auto textSize          = ImGui::CalcTextSize(open_label.c_str());
            ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
            ImGui::SetCursorPosY((windowSize.y - textSize.y) * 0.5f);
            ImGui::Text("%s", open_label.c_str());
            ImGui::SetCursorScreenPos(ImVec2(8, 28));
            if (ImGui::InvisibleButton("Open Video File", ImGui::GetContentRegionAvail())) {
                videoFileDialog.Open();
            }
            ImGui::SetItemAllowOverlap();
        } else {

            // Handle keyboard input
            if (!ImGui::IsPopupOpen(NULL, ImGuiPopupFlags_AnyPopup)) {
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                    video_file->pause(true);
                    if (ImGui::GetIO().KeyShift) {
                        video_file->seekRelative(-1.0);
                    }
                    else {
                        video_file->step(false);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                    video_file->pause(true);
                    if (ImGui::GetIO().KeyShift) {
                        video_file->seekRelative(1.0);
                    }
                    else {
                        video_file->step(true);
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
                    video_file->pause(!video_file->isPaused());
                }
            }

            auto windowSize = ImGui::GetWindowSize();
            auto textSize   = ImGui::CalcTextSize(filepath.c_str());
            ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
            ImGui::Text("%s", filepath.c_str());

            ImGui::SetNextItemAllowOverlap();

            ImVec2 uv_min     = ImVec2(0.0f, 0.0f);             // Top-left
            ImVec2 uv_max     = ImVec2(1.0f, 1.0f);             // Lower-right

            video_file->update();

            if (last_image_height <= 0) {
                last_image_height = video_file->getHeight();
            }

            float image_height = last_image_height + remaining_space.y;
            float image_width = video_file->getWidth() * image_height / video_file->getHeight();

            last_image_height = image_height;

            ImGui::SetCursorPosX((windowSize.x - image_width) * 0.5f);
            auto texture_id = video_file->getTextureId();
            ImGui::Image((void*)(intptr_t)texture_id, ImVec2(image_width, image_height), uv_min, uv_max);

            ImGui::PushItemWidth(-1);
            video_position = video_file->getPosition();
            seek_position  = video_position;
            ImGui::SliderFloat("##position", &seek_position, 0.0f, video_file->getDuration(), "%.3f s");
            if (!is_seeking && ImGui::IsItemActive()) {
                pause_for_seeking = !video_file->isPaused();
            }
            is_seeking = ImGui::IsItemActive();

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            for (size_t i = 0; i < annotation_classes.size(); i++) {
                std::string name_label = annotation_classes[i].name;
                std::string class_id_label = "class " + std::to_string(annotation_classes[i].id);
                if (name_label.empty()) {
                    name_label = class_id_label;
                }

                ImGui::PushID(class_id_label.c_str());
                if (MultiSpan(name_label.c_str(), annotations[i], seek_position, 0,
                              video_file->getDuration(), annotation_classes[i].color)) {
                    project->setDirty();
                    annotation_history.update(annotations);
                }
                ImGui::PopID();

                if (ImGui::BeginPopupContextItem(class_id_label.c_str())) {
                    if (ImGui::MenuItem("Clear")) {
                        annotations[i].clear();
                    }
                    if (ImGui::MenuItem("Edit Annotation Class")) {
                        edit_annotation_class = i;
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 5.0f));

            // play / pause button
            int play_button_dim = 48;
            ImGui::PushFont(fontawesome_large);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.0f);
            if (video_file && !video_file->isPaused()) {
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
                ImGui::SetCursorPosX((ImGui::GetWindowSize().x - play_button_dim) * 0.5f);
                if (ImGui::Button(ICON_FA_PAUSE, ImVec2(play_button_dim, play_button_dim))) {
                    video_file->pause(true);
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.6f, 0.5f));
                ImGui::SetCursorPosX((ImGui::GetWindowSize().x - play_button_dim) * 0.5f);
                if (ImGui::Button(ICON_FA_PLAY, ImVec2(play_button_dim, play_button_dim))) {
                    if (video_file) {
                        video_file->play();
                    }
                }
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleVar();
            ImGui::PopFont();

            // back / forward buttons
            int button_size = 32;
            ImGui::SameLine();
            ImGui::PushFont(fontawesome_medium);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.5f - 80.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (ImGui::Button(ICON_FA_STEP_BACKWARD, ImVec2(button_size, button_size))) {
                video_file->seek(0);
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.5f + 48.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
            if (ImGui::Button(ICON_FA_STEP_FORWARD, ImVec2(button_size, button_size))) {
                video_file->seek(video_file->getDuration());
            }
            ImGui::PopStyleVar();
            ImGui::PopFont();

            remaining_space = ImGui::GetContentRegionAvail();
        }

        videoFileDialog.Display();
        if (videoFileDialog.HasSelected() || !open_recent_video.empty()) {
            std::string video_path = open_recent_video;
            if (video_path.empty()) {
                video_path = videoFileDialog.GetSelected().string();
            }
            open_recent_video = {};

            videoFileDialog.ClearSelected();

            video_file = just_annotate::VideoFile::open(video_path);
            if (!video_file) {
                printf("Failed to open video file: %s\n", video_path.c_str());
                ImGui::OpenPopup("Error##LoadVideo");
            }
            else {
                filepath = video_path;
                config_state.addRecentVideo(video_path);
                saveConfig(config_state);
                std::string hash = get_file_hash(video_file->getPath());
                if (!hash.empty()) {

                    if (project->hasFile()) {
                        for (size_t i = 0; i < annotation_classes.size(); i++) {
                            if(!project->setAnnotations({annotation_classes[i].id, annotations[i]})) {
                                spdlog::error("Failed to set annotations.");
                            }
                            annotations[i].clear();
                        }
                    }

                    std::filesystem::path fs_path(video_path);
                    project->setFile(fs_path.filename().string(), hash);
                    auto saved_file_annotations = project->getAnnotations();
                    for (const auto& saved_annotations: saved_file_annotations) {
                        for (size_t i = 0; i < annotation_classes.size(); i++) {
                            if (annotation_classes[i].id == saved_annotations.id) {
                                annotations[i] = saved_annotations.spans;
                            }
                        }
                    }
                    annotation_history.initialize(annotations);
                }
            }
        }

        openProjectDialog.Display();
        if (openProjectDialog.HasSelected() || !open_recent_path.empty()) {
            std::string open_path = open_recent_path;
            if (open_path.empty()) {
                open_path = openProjectDialog.GetSelected().string();
            }

            int do_open = CONFIRM_UNKNOWN;
            if (project->isDirty()) {
                do_open = getConfirmation("Are you sure?##OpenProject", "Do you really want to close the current project without saving first?", fontawesome_large);
            }

            if (!project->isDirty() || do_open == CONFIRM_YES) {
                open_recent_path = {};
                openProjectDialog.ClearSelected();

                auto opened_project = AnnotationStore::open(open_path);
                if (!opened_project) {
                    spdlog::error("Failed to open project at: {}", open_path);
                    ImGui::OpenPopup("Error##OpenProject");
                }
                else {
                    project_path = open_path;
                    project = opened_project;

                    config_state.addRecentFile(project_path);
                    saveConfig(config_state);

                    annotation_classes = project->getAnnotationClasses();
                    annotationClassDialog.ClearExisting();
                    annotations.clear();
                    annotations.resize(annotation_classes.size());

                    for (const auto& annotation_class: annotation_classes) {
                        annotationClassDialog.AddExisting(annotation_class.id);
                    }

                    if (video_file) {
                        std::string hash = get_file_hash(video_file->getPath());
                        if (!hash.empty()) {
                            std::filesystem::path fs_path(video_file->getPath());
                            project->setFile(fs_path.filename().string(), hash);
                            auto saved_file_annotations = project->getAnnotations();
                            for (const auto& saved_annotations: saved_file_annotations) {
                                for (size_t i = 0; i < annotation_classes.size(); i++) {
                                    if (annotation_classes[i].id == saved_annotations.id) {
                                        annotations[i] = saved_annotations.spans;
                                    }
                                }
                            }
                        }
                        annotation_history.initialize(annotations);
                    }
                }
            }
            else if (do_open == CONFIRM_NO) {
                openProjectDialog.ClearSelected();
                open_recent_path = {};
            }
        }

        saveProjectDialog.Display();
        if (saveProjectDialog.HasSelected()) {
            std::string save_path = saveProjectDialog.GetSelected().string();
            saveProjectDialog.ClearSelected();

            for (size_t i = 0; i < annotation_classes.size(); i++) {
                if(!project->setAnnotations({annotation_classes[i].id, annotations[i]})) {
                    spdlog::error("Failed to set annotations.");
                }
            }

            if (project->save(save_path)) {
                project_path = save_path;
                config_state.addRecentFile(project_path);
                saveConfig(config_state);
            }
            else {
                ImGui::OpenPopup("Error##SaveProject");
            }
        }

        if (new_project) {
            int do_create_new = CONFIRM_UNKNOWN;
            if (project->isDirty()) {
                do_create_new = getConfirmation("Are you sure?##NewProject", "Do you really want to close the current project without saving first?", fontawesome_large);
            }

            if (!project->isDirty() || do_create_new == CONFIRM_YES) {
                new_project = false;
                project_path = {};
                annotation_classes.clear();
                annotations.clear();
                annotation_history.clear();
                annotationClassDialog.ClearExisting();
                project = std::make_shared<AnnotationStore>();

                if (video_file) {
                    std::string hash = get_file_hash(video_file->getPath());
                    if (!hash.empty()) {
                        std::filesystem::path fs_path(video_file->getPath());
                        project->setFile(fs_path.filename().string(), hash);
                    }
                }
            }
            else if (do_create_new == CONFIRM_NO) {
                new_project = false;
            }
        }

        if (add_annotation_class) {
            add_annotation_class = false;
            annotationClassDialog.SetTitle("Add Annotation Class");
            annotationClassDialog.Open();
        }

        if (edit_annotation_class >= 0) {
            annotationClassDialog.SetTitle("Edit Annotation Class");
            annotationClassDialog.Edit(annotation_classes[edit_annotation_class], edit_annotation_class);
            edit_annotation_class = -1;
        }

        annotationClassDialog.Display(fontawesome_large);
        if (annotationClassDialog.HasAnnotationClass()) {
            if (!project->addAnnotationClass(annotationClassDialog.GetAnnotationClass())) {
                spdlog::error("Failed to add annotation class.");
                ImGui::OpenPopup("Error##AddAnnotationClass");
            }
            annotation_classes.push_back(annotationClassDialog.GetAnnotationClass());
            annotations.push_back({});
            annotation_history.initialize(annotations);
            annotationClassDialog.Clear();
        }
        else if (annotationClassDialog.DeletedAnnotationClass()) {
            if (!project->deleteAnnotationClass(annotation_classes[annotationClassDialog.GetEditIndex()].id)) {
                spdlog::error("Failed to add annotation class.");
                ImGui::OpenPopup("Error##DeleteAnnotationClass");
            }
            annotation_classes.erase(annotation_classes.begin() + annotationClassDialog.GetEditIndex());
            annotations.erase(annotations.begin() + annotationClassDialog.GetEditIndex());
            annotation_history.initialize(annotations);
            annotationClassDialog.Clear();
        }
        else if (annotationClassDialog.ModifiedAnnotationClass()) {
            if (!project->modifyAnnotationClass(annotation_classes[annotationClassDialog.GetEditIndex()].id, annotationClassDialog.GetAnnotationClass())) {
                spdlog::error("Failed to modify annotation class.");
                ImGui::OpenPopup("Error##ModifyAnnotationClass");
            }
            annotation_classes[annotationClassDialog.GetEditIndex()] = annotationClassDialog.GetAnnotationClass();
            annotationClassDialog.Clear();
        }

        if (try_exit) {
            int do_exit = CONFIRM_UNKNOWN;
            if (project->isDirty()) {
                do_exit = getConfirmation("Are you sure?##Exit", "Do you really want to close the current project without saving first?", fontawesome_large);
            }

            if (!project->isDirty() || do_exit == CONFIRM_YES) {
                try_exit = false;
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            else if (do_exit == CONFIRM_NO) {
                try_exit = false;
            }
        }

        displayErrorMessage("Error##LoadVideo", "Failed to open video file.", fontawesome_large);
        displayErrorMessage("Error##Hash", "Failed to get hash of video file.", fontawesome_large);
        displayErrorMessage("Error##OpenProject", "Failed to open project.", fontawesome_large);
        displayErrorMessage("Error##SaveProject", "Failed to save project.", fontawesome_large);
        displayErrorMessage("Error##AddAnnotationClass", "Failed to add annotation class.", fontawesome_large);
        displayErrorMessage("Error##DeleteAnnotationClass", "Failed to delete annotation class.", fontawesome_large);
        displayErrorMessage("Error##ModifyAnnotationClass", "Failed to modify annotation class.", fontawesome_large);

        ImGui::End();

        // Rendering
        ImGui::Render();
        ImVec4 clear_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);

        if (seek_position != video_position) {
            video_file->seek(seek_position);
        }
        if (is_seeking && !video_file->isPaused()) {
            video_file->pause(true);
        }
        else if (!is_seeking && pause_for_seeking) {
            video_file->pause(false);
            pause_for_seeking = false;
        }

        std::string title = project_path;
        if (title.empty()) {
            title = "New Project";
        }
        if (project->isDirty()) {
            title += "*";
        }
        title = "JustAnnotate:  " + title;
        if (title != last_title) {
            glfwSetWindowTitle(window, title.c_str());
            last_title = title;
        }
        glfwGetWindowSize(window, &config_state.window_width, &config_state.window_height);
        glfwGetWindowPos(window, &config_state.window_x, &config_state.window_y);

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - last_config_save;
        if (elapsed.count() >= 1.0 && config_state != last_config_saved) {
            last_config_save = now;
            last_config_saved = config_state;
            saveConfig(config_state);
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    delete[] fa_ttf_data;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
