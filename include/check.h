#pragma once
#include "common.h"
#include "ui.hpp"
#include <map>

class Renderer;
struct FontSet;

enum class IssueSeverity { Info, Warning, Error };
struct CheckIssue {
    int slide = 0; // One-based
    std::string element, source, code, message, suggestion;
    IssueSeverity severity = IssueSeverity::Info;
    ui::Rect bounds;
    std::map<std::string, double> measurements;
};
struct CheckReport {
    int slidesChecked = 0;
    std::vector<CheckIssue> issues;
    int count(IssueSeverity severity) const;
    bool failsStrict() const;
};

// selectedSlide=0 checks the complete deck. No rendering or window is needed.
CheckReport checkPresentation(const Presentation& presentation, const FontSet& fonts,
                              Renderer& renderer, int selectedSlide = 0);
std::string formatCheckReport(const CheckReport& report, bool json);
