#pragma once

#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

enum ConfirmationResponse
{
    CONFIRM_UNKNOWN = 0,
    CONFIRM_YES = 1,
    CONFIRM_NO = 2
};

void centerText(const std::string& text);

void displayErrorMessage(const std::string& title, const std::string& message, ImFont* icons=nullptr);

int getConfirmation(const std::string& title, const std::string& message, ImFont* icons = nullptr);

