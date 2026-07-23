#include "highlight.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> splitWS(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string w;
    while (ss >> w) out.push_back(w);
    return out;
}

LanguageSpec loadLanguage(const std::string& langName) {
    LanguageSpec spec;
    spec.name = langName;
    spec.stringChars = "\"'";

    std::string path = "share/highlight/" + langName + ".txt";
    std::ifstream f(path);
    if (!f.is_open()) return spec;

    std::string line;
    std::string section;
    std::string accum;

    auto flush = [&]() {
        if (accum.empty()) return;
        if (section == "keywords") {
            for (auto& w : splitWS(accum)) spec.keywords.insert(w);
        } else if (section == "types") {
            for (auto& w : splitWS(accum)) spec.types.insert(w);
        } else if (section == "builtins") {
            for (auto& w : splitWS(accum)) spec.builtins.insert(w);
        }
        accum.clear();
    };

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[' && line.back() == ']') {
            flush();
            section = line.substr(1, line.size() - 2);
            if (section == "config") section = "config";
            continue;
        }

        if (section == "config") {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = trim(line.substr(0, eq));
                std::string val = trim(line.substr(eq + 1));
                if (key == "line_comment") spec.lineComment = val;
                else if (key == "block_comment_start") spec.blockCommentStart = val;
                else if (key == "block_comment_end") spec.blockCommentEnd = val;
                else if (key == "string_chars") spec.stringChars = val;
                else if (key == "brackets") {
                    std::istringstream rs(val);
                    std::string rule;
                    while (std::getline(rs, rule, ',')) {
                        rule = trim(rule);
                        if (rule.empty()) continue;
                        auto parts = splitWS(rule);
                        if (parts.size() < 3) continue;
                        BracketRule br;
                        br.open  = parts[0];
                        br.close = parts[1];
                        br.tagMode = (parts.back() == "tag");
                        spec.brackets.push_back(br);
                    }
                }
            }
        } else if (section == "keywords" || section == "types" || section == "builtins") {
            if (!accum.empty()) accum += " ";
            accum += line;
        }
    }
    flush();
    return spec;
}

static bool isIdentStart(int c) {
    return std::isalpha(c) || c == '_';
}

static bool isIdentPart(int c) {
    return std::isalnum(c) || c == '_';
}

static bool isLispIdentStart(int c) {
    return std::isalpha(c) || c == '_' ||
           c == '+' || c == '-' || c == '*' || c == '/' ||
           c == '<' || c == '>' || c == '=' ||
           c == '!' || c == '?' || c == '$' || c == '%' ||
           c == '&' || c == '~' || c == '^' || c == '@' || c == ':';
}

static bool isLispIdentPart(int c) {
    return isLispIdentStart(c) || std::isdigit(c) || c == '.';
}

static const BracketRule* findOpenBracket(const LanguageSpec& spec, const std::string& code, size_t pos) {
    for (const auto& br : spec.brackets) {
        if (code.compare(pos, br.open.size(), br.open) == 0) return &br;
    }
    return nullptr;
}

static bool hasLispMode(const LanguageSpec& spec) {
    for (const auto& br : spec.brackets)
        if (!br.tagMode) return true;
    return false;
}

static bool startsWith(const std::string& s, size_t pos, const std::string& prefix) {
    if (pos + prefix.size() > s.size()) return false;
    return s.compare(pos, prefix.size(), prefix) == 0;
}

std::vector<HighlightToken> tokenize(const std::string& code, const LanguageSpec& spec) {
    std::vector<HighlightToken> tokens;
    size_t i = 0;
    size_t n = code.size();
    std::string lc = spec.lineComment;
    std::string bcs = spec.blockCommentStart;
    std::string bce = spec.blockCommentEnd;
    bool expectHead = false;

    while (i < n) {
        unsigned char c = static_cast<unsigned char>(code[i]);

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            tokens.push_back({i, 1, HighlightType::Other});
            i++;
            continue;
        }

        if (!lc.empty() && startsWith(code, i, lc)) {
            size_t end = code.find('\n', i);
            if (end == std::string::npos) end = n;
            tokens.push_back({i, end - i, HighlightType::Comment});
            i = end;
            continue;
        }

        if (!bcs.empty() && startsWith(code, i, bcs)) {
            size_t end = code.find(bce, i + bcs.size());
            if (end == std::string::npos) end = n;
            else end += bce.size();
            tokens.push_back({i, end - i, HighlightType::Comment});
            i = end;
            continue;
        }

        if (spec.stringChars.find(c) != std::string::npos) {
            char delim = static_cast<char>(c);
            size_t start = i;
            i++;
            while (i < n) {
                if (code[i] == '\\') { i += 2; continue; }
                if (code[i] == delim) { i++; break; }
                i++;
            }
            tokens.push_back({start, i - start, HighlightType::String});
            continue;
        }

        const BracketRule* openBr = findOpenBracket(spec, code, i);
        if (openBr && openBr->tagMode && openBr->open == "<") {
            tokens.push_back({i, 1, HighlightType::Punctuation});
            i++;
            if (i < n && code[i] == '/') {
                tokens.push_back({i, 1, HighlightType::Punctuation});
                i++;
            }
            if (i < n && isIdentStart(code[i])) {
                size_t ns = i;
                while (i < n && isIdentPart(code[i])) i++;
                tokens.push_back({ns, i - ns, HighlightType::Keyword});
            }
            while (i < n && code[i] != '>') {
                while (i < n && (code[i] == ' ' || code[i] == '\t' || code[i] == '\r' || code[i] == '\n')) {
                    tokens.push_back({i, 1, HighlightType::Other});
                    i++;
                }
                if (i >= n || code[i] == '>') break;
                if (code[i] == '/') {
                    tokens.push_back({i, 1, HighlightType::Punctuation});
                    i++;
                    break;
                }
                if (isIdentStart(code[i])) {
                    size_t as = i;
                    while (i < n && isIdentPart(code[i])) i++;
                    tokens.push_back({as, i - as, HighlightType::Type});
                } else {
                    tokens.push_back({i, 1, HighlightType::Other});
                    i++;
                    continue;
                }
                while (i < n && (code[i] == ' ' || code[i] == '\t' || code[i] == '\r' || code[i] == '\n')) {
                    tokens.push_back({i, 1, HighlightType::Other});
                    i++;
                }
                if (i < n && code[i] == '=') {
                    tokens.push_back({i, 1, HighlightType::Other});
                    i++;
                }
                while (i < n && (code[i] == ' ' || code[i] == '\t' || code[i] == '\r' || code[i] == '\n')) {
                    tokens.push_back({i, 1, HighlightType::Other});
                    i++;
                }
                if (i < n && (code[i] == '"' || code[i] == '\'')) {
                    char delim = code[i];
                    size_t ss = i;
                    i++;
                    while (i < n) {
                        if (code[i] == '\\') { i += 2; continue; }
                        if (code[i] == delim) { i++; break; }
                        i++;
                    }
                    tokens.push_back({ss, i - ss, HighlightType::String});
                }
            }
            if (i < n && code[i] == '>') {
                tokens.push_back({i, 1, HighlightType::Punctuation});
                i++;
            }
            continue;
        }

        if (openBr && !openBr->tagMode) {
            tokens.push_back({i, openBr->open.size(), HighlightType::Punctuation});
            i += openBr->open.size();
            expectHead = true;
            continue;
        }

        {
            const BracketRule* closeBr = nullptr;
            for (const auto& br : spec.brackets) {
                if (code.compare(i, br.close.size(), br.close) == 0) {
                    closeBr = &br;
                    break;
                }
            }
            if (closeBr && !closeBr->tagMode && !expectHead) {
                tokens.push_back({i, closeBr->close.size(), HighlightType::Punctuation});
                i += closeBr->close.size();
                continue;
            }
        }

        if (std::isdigit(c) || (c == '.' && i + 1 < n && std::isdigit(code[i + 1]))) {
            size_t start = i;
            if (c == '0' && i + 1 < n && (code[i + 1] == 'x' || code[i + 1] == 'X')) {
                i += 2;
                while (i < n && (std::isxdigit(code[i]))) i++;
            } else {
                while (i < n && (std::isdigit(code[i]) || code[i] == '.' ||
                       code[i] == 'e' || code[i] == 'E' ||
                       code[i] == '+' || code[i] == '-' ||
                       code[i] == 'f' || code[i] == 'F' ||
                       code[i] == 'x' || code[i] == 'X')) {
                    i++;
                }
            }
            tokens.push_back({start, i - start, HighlightType::Number});
            continue;
        }

        bool useLispIdent = hasLispMode(spec);
        bool identStart = useLispIdent ? isLispIdentStart(c) : isIdentStart(c);
        if (identStart) {
            size_t start = i;
            while (i < n && (useLispIdent ? isLispIdentPart(code[i]) : isIdentPart(code[i]))) i++;
            std::string word = code.substr(start, i - start);

            HighlightType t = HighlightType::Other;
            if (expectHead) { t = HighlightType::Keyword; expectHead = false; }
            else if (spec.keywords.count(word)) t = HighlightType::Keyword;
            else if (spec.types.count(word)) t = HighlightType::Type;
            else if (spec.builtins.count(word)) t = HighlightType::Builtin;

            tokens.push_back({start, i - start, t});
            continue;
        }

        tokens.push_back({i, 1, HighlightType::Other});
        i++;
    }

    return tokens;
}
