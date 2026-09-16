#include "parser.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <unistd.h>

int main() {
    char temp[] = "/tmp/presenter-parser-XXXXXX";
    auto* directory = mkdtemp(temp); assert(directory);
    auto path = std::filesystem::path(directory) / "presentation.xml";
    auto parse = [&](const std::string& xml) {
        { std::ofstream out(path); out << xml; }
        return parseXml(directory);
    };
    auto p = parse(R"(<presentation name="Manual"><style><fonts content="26"/></style>
      <slide title="Metadata"><notes>Private</notes>
        <grid rows="auto *" columns="2* *" gap="20">
          <text columnSpan="2" role="title"><b>Actual</b> heading</text>
          <stack row="1" gap="12"><text>First</text><image src="images/a.png"/>
            <text>Last <code>inline</code></text></stack>
          <chart row="1" column="1" type="line"><point label="A" value="12.5"/></chart>
        </grid>
      </slide></presentation>)");
    assert(p.size() == 1 && p.name == "Manual" && p.style.contentFontSize == 26);
    assert(p.slides[0].title == "Metadata" && p.slides[0].notes == "Private");
    const auto& root = p.slides[0].elements[0];
    assert(root.children[0].text == "<b>Actual</b> heading");
    assert(root.children[1].children[1].attributes.at("src") == (std::filesystem::path(directory) / "images/a.png").string());
    assert(root.children[1].children[2].text == "Last <code>inline</code>");
    assert(root.children[2].chart.type == ChartType::Line && root.children[2].chart.points[0].value == 12.5);
    for (const auto& theme : PresentationStyle::builtInThemes()) assert(theme.imageCornerRadius == 0);
    auto rounded = parse(R"(<presentation><style theme="Studio"><layout imageCornerRadius="18"/></style>
        <slide><stack/></slide></presentation>)");
    assert(rounded.style.imageCornerRadius == 18);
    assert(rounded.style.cornerRadius == PresentationStyle::defaults().cornerRadius);
    auto square = parse(R"(<presentation><style><layout cornerRadius="40"/></style>
        <slide><stack/></slide></presentation>)");
    assert(square.style.imageCornerRadius == 0 && square.style.cornerRadius == 40);
    auto negative = parse(R"(<presentation><style><layout imageCornerRadius="-5"/></style>
        <slide><stack/></slide></presentation>)");
    assert(negative.style.imageCornerRadius == 0);
    {
        std::ofstream file(std::filesystem::path(directory) / "rounded.style");
        file << "<style><layout imageCornerRadius='22'/></style>";
    }
    auto external = parse("<presentation style='rounded.style'><slide><stack/></slide></presentation>");
    assert(external.style.imageCornerRadius == 22);
    for (const char* invalid : {
        "<slide layout='title'><stack/></slide>",
        "<slide><text>missing root</text></slide>",
        "<slide><stack/><grid/></slide>",
        "<slide><stack><slide/></stack></slide>",
        "<slide><grid columns='bad'/></slide>",
        "<slide><grid><text column='1'>out of range</text></grid></slide>",
        "<slide><stack gap='-1'/></slide>",
        "<slide><stack><text width='nan'/></stack></slide>",
        "<slide><border><text/><text/></border></slide>",
        "<slide><stack><image/></stack></slide>",
        "<slide><stack typo='yes'/></slide>"}) {
        assert(parse(std::string("<presentation>") + invalid + "</presentation>").empty());
    }
    assert(!parseXml("demo/Nature Portfolio.slides").empty());
    assert(!parseXml("demo/Theme Studio.slides").empty());
    std::filesystem::remove_all(directory);
    puts("parser: ordered tree, package paths, styles, charts, rejection checks passed");
}
