CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -Iinclude -Ithird_party $(shell pkg-config --cflags sdl2)

LDFLAGS  ?=
LDLIBS    = $(shell pkg-config --libs sdl2)

BUILD     = build

# --- sources ---------------------------------------------------------------

CORE_SRC = src/xml_parser.cpp src/style.cpp src/font.cpp src/renderer.cpp \
           src/layout.cpp src/image.cpp src/highlight.cpp src/ui.cpp \
           src/screenshot.cpp src/charts.cpp

THIRD_SRC = third_party/tinyxml2.cpp

# --- targets ---------------------------------------------------------------

.PHONY: all clean demo test

all: $(BUILD)/presenter $(BUILD)/test_textbounds $(BUILD)/test_layout \
     $(BUILD)/test_xml_parser $(BUILD)/test_image $(BUILD)/test_highlight \
     $(BUILD)/test_screenshot
	@cp -r assets $(BUILD)/
	@cp -r share $(BUILD)/

$(BUILD):
	@mkdir -p $(BUILD)

# main presenter executable
$(BUILD)/presenter: src/main.cpp $(CORE_SRC) $(THIRD_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(CORE_SRC) $(THIRD_SRC) $(LDFLAGS) $(LDLIBS)

# --- tests -----------------------------------------------------------------

$(BUILD)/test_textbounds: test/test_textbounds.cpp $(CORE_SRC) $(THIRD_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE_SRC) $(THIRD_SRC) $(LDFLAGS) $(LDLIBS)

$(BUILD)/test_layout: test/test_layout.cpp src/layout.cpp src/style.cpp $(THIRD_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< src/layout.cpp src/style.cpp $(THIRD_SRC) $(LDFLAGS) $(LDLIBS)

$(BUILD)/test_xml_parser: test/test_xml_parser.cpp src/xml_parser.cpp src/style.cpp $(THIRD_SRC) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< src/xml_parser.cpp src/style.cpp $(THIRD_SRC) $(LDFLAGS) $(LDLIBS)

$(BUILD)/test_image: test/test_image.cpp src/image.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< src/image.cpp $(LDFLAGS) $(LDLIBS)

$(BUILD)/test_highlight: test/test_highlight.cpp src/highlight.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< src/highlight.cpp $(LDFLAGS) $(LDLIBS)

$(BUILD)/test_screenshot: test/test_screenshot.cpp src/screenshot.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $< src/screenshot.cpp $(LDFLAGS) $(LDLIBS)

# --- convenience ------------------------------------------------------------

demo: $(BUILD)/presenter
	./$(BUILD)/presenter "demo/Nature Portfolio.slides"

clean:
	rm -rf $(BUILD)
