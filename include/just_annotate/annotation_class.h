#pragma once

#include <string>

#include <imgui.h>

struct AnnotationClass {
    int id = 0;
    std::string name;
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
};
