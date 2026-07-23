#pragma once
#include <string>
#include <vector>
#include <unordered_set>

enum class HighlightType {
    Keyword,
    Type,
    String,
    Comment,
    Number,
    Builtin,
    Punctuation,
    Other
};

struct HighlightToken {
    size_t start;
    size_t len;
    HighlightType type;
};

struct BracketRule {
    std::string open;
    std::string close;
    bool tagMode = false; // true=XML tag with attributes, false=lisp head-word
};

struct LanguageSpec {
    std::string name;
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;
    std::unordered_set<std::string> builtins;
    std::string lineComment;
    std::string blockCommentStart;
    std::string blockCommentEnd;
    std::string stringChars;
    std::vector<BracketRule> brackets;
};

LanguageSpec loadLanguage(const std::string& langName);
std::vector<HighlightToken> tokenize(const std::string& code, const LanguageSpec& spec);
