#include "parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

static SlideType parseSlideType(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "title") return SlideType::Title;
    if (lower == "content") return SlideType::Content;
    if (lower == "image") return SlideType::Image;
    if (lower == "section") return SlideType::Section;
    if (lower == "two-column") return SlideType::TwoColumn;
    return SlideType::Content;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

Presentation parseMarkdown(const std::string& filePath) {
    Presentation pres;
    std::ifstream file(filePath);
    if (!file.is_open()) return pres;

    std::filesystem::path mdDir = std::filesystem::path(filePath).parent_path();

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    // Split into slide blocks by # heading at start of line
    struct Block { std::vector<std::string> lines; };
    std::vector<Block> slideBlocks;

    auto isFrontMatter = [](const std::string& t) -> bool {
        std::string lower = t;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.rfind("layout:", 0) == 0 || lower.rfind("notes:", 0) == 0;
    };

    auto isHeading = [](const std::string& t) -> bool {
        return t.rfind("# ", 0) == 0;
    };

    std::vector<std::string> currentBlock;
    bool hasHeading = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string t = trim(lines[i]);

        // Front matter after heading content: start new slide
        if (isFrontMatter(t) && hasHeading) {
            slideBlocks.push_back({currentBlock});
            currentBlock.clear();
            hasHeading = false;
        }

        if (isHeading(t)) {
            if (hasHeading) {
                slideBlocks.push_back({currentBlock});
                currentBlock.clear();
            }
            hasHeading = true;
        }

        currentBlock.push_back(lines[i]);
    }
    if (!currentBlock.empty()) {
        slideBlocks.push_back({currentBlock});
    }

    std::regex imageRegex(R"(!\[([^\]]*)\]\(([^)]+)\))");

    for (auto& block : slideBlocks) {
        Slide slide;
        std::string layoutStr;
        bool hasLayout = false;

        // Parse front matter (layout:/notes: lines before the # heading)
        size_t contentStart = 0;
        for (size_t i = 0; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            std::string lower = t;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower.rfind("layout:", 0) == 0) {
                layoutStr = trim(t.substr(7));
                hasLayout = true;
                contentStart = i + 1;
            } else if (lower.rfind("notes:", 0) == 0) {
                slide.notes = trim(t.substr(6));
                contentStart = i + 1;
            } else {
                break;
            }
        }

        if (hasLayout) {
            slide.type = parseSlideType(layoutStr);
        }

        // Find the # heading
        std::string heading;
        size_t headingIdx = contentStart;
        for (size_t i = contentStart; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            if (t.rfind("# ", 0) == 0) {
                heading = t.substr(2);
                headingIdx = i + 1;
                break;
            }
        }
        slide.title = heading;

        // Collect non-empty content lines after heading
        std::vector<std::string> contentLines;
        for (size_t i = headingIdx; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            if (!t.empty()) {
                contentLines.push_back(t);
            }
        }

        switch (slide.type) {
            case SlideType::Title: {
                // Remaining lines are subtitle
                for (auto& l : contentLines) {
                    if (l == slide.title) continue;
                    slide.bullets.push_back(l);
                }
                if (!slide.bullets.empty()) {
                    slide.imageAlt = slide.bullets[0];
                    slide.bullets.clear();
                }
                break;
            }
            case SlideType::Content: {
                for (auto& l : contentLines) {
                    if (l.rfind("- ", 0) == 0) {
                        slide.bullets.push_back(l.substr(2));
                    } else if (!l.empty()) {
                        slide.bullets.push_back(l);
                    }
                }
                break;
            }
            case SlideType::Image: {
                bool foundImage = false;
                for (auto& l : contentLines) {
                    std::smatch match;
                    if (!foundImage && std::regex_match(l, match, imageRegex)) {
                        slide.imageAlt = match[1].str();
                        std::string relPath = match[2].str();
                        slide.imagePath = (mdDir / relPath).string();
                        foundImage = true;
                    } else {
                        slide.bullets.push_back(l);
                    }
                }
                break;
            }
            case SlideType::Section: {
                break;
            }
            case SlideType::TwoColumn: {
                enum class ColumnState { None, Left, Right };
                ColumnState state = ColumnState::None;
                for (auto& l : contentLines) {
                    if (l == "---left---") {
                        state = ColumnState::Left;
                        continue;
                    } else if (l == "---right---") {
                        state = ColumnState::Right;
                        continue;
                    }
                    if (state == ColumnState::Left) {
                        if (!slide.leftContent.empty()) slide.leftContent += "\n";
                        slide.leftContent += l;
                    } else if (state == ColumnState::Right) {
                        if (!slide.rightContent.empty()) slide.rightContent += "\n";
                        slide.rightContent += l;
                    }
                }
                break;
            }
        }

        pres.slides.push_back(slide);
    }

    return pres;
}
