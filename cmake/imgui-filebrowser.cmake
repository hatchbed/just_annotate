include(FetchContent)
FetchContent_Declare(
    imgui-filebrowser
    GIT_REPOSITORY https://github.com/AirGuanZ/imgui-filebrowser.git
    GIT_TAG 979acb7d7a9b5c0de4ee75b432d6222baf49d39f # May 31, 2024
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(imgui-filebrowser)
