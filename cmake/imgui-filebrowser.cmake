include(FetchContent)
FetchContent_Declare(
    imgui-filebrowser
    GIT_REPOSITORY https://github.com/AirGuanZ/imgui-filebrowser.git
    GIT_TAG 17a87fedfa997fb2b504a6b3088cc44e64fcac6a # May 31, 2024
    GIT_SHALLOW 1
)
FetchContent_MakeAvailable(imgui-filebrowser)
