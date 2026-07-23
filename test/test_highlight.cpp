#include "highlight.h"
#include <cstdio>

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("OK:   %s\n", msg); } \
} while(0)

void test_tokenize_cpp() {
    LanguageSpec spec = loadLanguage("cpp");
    CHECK(!spec.keywords.empty(), "cpp spec has keywords");
    CHECK(spec.keywords.count("if"), "cpp keyword 'if' found");
    CHECK(spec.types.count("int"), "cpp type 'int' found");

    auto tokens = tokenize("int x = 42;", spec);
    CHECK(!tokens.empty(), "tokenized cpp code");

    // int -> Type
    bool hasType = false;
    bool hasNum = false;
    for (auto& t : tokens) {
        if (t.type == HighlightType::Type) hasType = true;
        if (t.type == HighlightType::Number) hasNum = true;
    }
    CHECK(hasType, "found Type token");
    CHECK(hasNum, "found Number token");
}

void test_tokenize_python() {
    LanguageSpec spec = loadLanguage("python");
    CHECK(spec.keywords.count("def"), "python keyword 'def'");
    CHECK(spec.builtins.count("print"), "python builtin 'print'");

    auto tokens = tokenize("def hello():", spec);
    bool hasKeyword = false;
    for (auto& t : tokens) {
        if (t.type == HighlightType::Keyword) hasKeyword = true;
    }
    CHECK(hasKeyword, "found keyword in python def");
}

void test_comment() {
    LanguageSpec spec = loadLanguage("cpp");
    auto tokens = tokenize("// this is a comment\nint x;", spec);
    bool hasComment = false;
    for (auto& t : tokens) {
        if (t.type == HighlightType::Comment) { hasComment = true; break; }
    }
    CHECK(hasComment, "found line comment");
}

void test_string() {
    LanguageSpec spec = loadLanguage("python");
    auto tokens = tokenize("x = \"hello world\"", spec);
    bool hasString = false;
    for (auto& t : tokens) {
        if (t.type == HighlightType::String) { hasString = true; break; }
    }
    CHECK(hasString, "found string token");
}

void test_missing_lang() {
    LanguageSpec spec = loadLanguage("nonexistent");
    CHECK(spec.keywords.empty(), "missing lang has no keywords");
    CHECK(spec.name == "nonexistent", "name preserved for missing lang");
}

void test_block_comment() {
    LanguageSpec spec = loadLanguage("cpp");
    auto tokens = tokenize("/* block\ncomment */\nint x;", spec);
    bool hasComment = false;
    for (auto& t : tokens) {
        if (t.type == HighlightType::Comment) { hasComment = true; break; }
    }
    CHECK(hasComment, "found block comment");
}

int main() {
    test_tokenize_cpp();
    test_tokenize_python();
    test_comment();
    test_string();
    test_missing_lang();
    test_block_comment();
    printf("\n%d failures\n", failures);
    return failures;
}
