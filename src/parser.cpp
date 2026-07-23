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
    if (lower == "two-column" || lower == "twocolumn") return SlideType::TwoColumn;
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
        return lower.rfind("layout:", 0) == 0 || lower.rfind("notes:", 0) == 0 || lower.rfind("fit:", 0) == 0;
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

        // Parse front matter (layout:/notes:/fit: lines before the # heading)
        size_t contentStart = 0;
        for (size_t i = 0; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            std::string lower = t;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (t.empty()) {
                // skip blank lines within front matter region
            } else if (lower.rfind("layout:", 0) == 0) {
                layoutStr = trim(t.substr(7));
                hasLayout = true;
                slide.layoutSpecified = true;
                contentStart = i + 1;
            } else if (lower.rfind("notes:", 0) == 0) {
                slide.notes = trim(t.substr(6));
                contentStart = i + 1;
            } else if (lower.rfind("fit:", 0) == 0) {
                std::string fitStr = trim(t.substr(4));
                std::transform(fitStr.begin(), fitStr.end(), fitStr.begin(), ::tolower);
                if (fitStr == "fill") {
                    slide.imageFit = ImageFit::Fill;
                } else {
                    slide.imageFit = ImageFit::Fit;
                }
                contentStart = i + 1;
            } else {
                break;
            }
        }

        slide.type = hasLayout ? parseSlideType(layoutStr) : SlideType::Content;

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

        // Collect non-empty content lines after heading (skip '---' slide separators)
        std::vector<std::string> contentLines;
        for (size_t i = headingIdx; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            if (!t.empty() && t != "---") {
                contentLines.push_back(t);
            }
        }

        // Generic extraction: images, columns, subtitle, bullets for all slide types
        bool foundImage = false;
        enum class ColumnState { None, Left, Right };
        ColumnState columnState = ColumnState::None;

        for (auto& l : contentLines) {
            // Skip title line itself
            if (l == slide.title) continue;

            // Check for column separators
            if (l == "---left---") {
                columnState = ColumnState::Left;
                continue;
            } else if (l == "---right---") {
                columnState = ColumnState::Right;
                continue;
            }

            // Handle column content
            if (columnState == ColumnState::Left) {
                if (!slide.leftContent.empty()) slide.leftContent += "\n";
                slide.leftContent += l;
                continue;
            } else if (columnState == ColumnState::Right) {
                if (!slide.rightContent.empty()) slide.rightContent += "\n";
                slide.rightContent += l;
                continue;
            }

            // Check for image markdown
            std::smatch match;
            if (!foundImage && std::regex_match(l, match, imageRegex)) {
                slide.imageAlt = match[1].str();
                std::string relPath = match[2].str();
                slide.imagePath = (mdDir / relPath).lexically_normal().string();
                foundImage = true;
                continue;
            }

            // Handle different slide types
            switch (slide.type) {
                case SlideType::Title: {
                    // Store as subtitle (first non-title line)
                    if (slide.subtitle.empty()) {
                        slide.subtitle = l;
                        slide.imageAlt = l; // backward compatibility
                    }
                    break;
                }
                case SlideType::Content: {
                    // Bullet points
                    if (l.rfind("- ", 0) == 0) {
                        slide.bullets.push_back(l.substr(2));
                    } else {
                        slide.bullets.push_back(l);
                    }
                    break;
                }
                case SlideType::Image: {
                    // Non-image lines become bullets (captions)
                    slide.bullets.push_back(l);
                    break;
                }
                case SlideType::Section: {
                    // Section slides typically have no content
                    break;
                }
                case SlideType::TwoColumn: {
                    // Column separators already handled above
                    break;
                }
            }
        }

        pres.slides.push_back(slide);
    }

    return pres;
}
