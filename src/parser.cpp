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

        // Collect non-empty content lines after heading (except --- block separators)
        std::vector<std::string> contentLines;
        for (size_t i = headingIdx; i < block.lines.size(); ++i) {
            std::string t = trim(block.lines[i]);
            if (!t.empty()) {
                contentLines.push_back(t);
            }
        }

        // Split content on --- into blocks, scanning for images inline
        std::vector<std::string> rawBlocks;
        std::vector<std::string> currentBlockLines;
        bool foundImage = false;

        for (auto& l : contentLines) {
            if (l == slide.title) continue;

            // Detect images from individual lines
            std::smatch match;
            if (!foundImage && std::regex_match(l, match, imageRegex)) {
                slide.imageAlt = match[1].str();
                std::string relPath = match[2].str();
                slide.imagePath = (mdDir / relPath).lexically_normal().string();
                foundImage = true;
                continue;
            }

            if (l == "---") {
                if (!currentBlockLines.empty()) {
                    std::string joined;
                    for (size_t i = 0; i < currentBlockLines.size(); ++i) {
                        if (i > 0) joined += "\n";
                        joined += currentBlockLines[i];
                    }
                    rawBlocks.push_back(joined);
                    currentBlockLines.clear();
                }
            } else {
                currentBlockLines.push_back(l);
            }
        }
        if (!currentBlockLines.empty()) {
            std::string joined;
            for (size_t i = 0; i < currentBlockLines.size(); ++i) {
                if (i > 0) joined += "\n";
                joined += currentBlockLines[i];
            }
            rawBlocks.push_back(joined);
            currentBlockLines.clear();
        }

        for (auto& raw : rawBlocks) {
            slide.blocks.push_back(raw);
        }

        // For explicit layouts, handle content accordingly
        switch (slide.type) {
            case SlideType::Title: {
                if (!slide.blocks.empty() && slide.subtitle.empty()) {
                    slide.subtitle = slide.blocks[0];
                    slide.imageAlt = slide.blocks[0];
                }
                break;
            }
            case SlideType::Image: {
                for (auto& b : slide.blocks) {
                    slide.bullets.push_back(b);
                }
                break;
            }
            case SlideType::Content:
            case SlideType::TwoColumn:
            default: {
                if (foundImage) {
                    // Text blocks become captions for the image
                    for (auto& b : slide.blocks) {
                        slide.bullets.push_back(b);
                    }
                } else if (slide.blocks.size() == 1) {
                    std::istringstream iss(slide.blocks[0]);
                    std::string line;
                    while (std::getline(iss, line)) {
                        line = trim(line);
                        if (!line.empty()) {
                            if (line.rfind("- ", 0) == 0) {
                                slide.bullets.push_back(line.substr(2));
                            } else {
                                slide.bullets.push_back(line);
                            }
                        }
                    }
                }
                break;
            }
            case SlideType::Section: {
                break;
            }
        }

        pres.slides.push_back(slide);
    }

    return pres;
}
