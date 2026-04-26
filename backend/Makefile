CXX ?= clang++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS ?=

BUILD_DIR := build
TARGET := $(BUILD_DIR)/defect-intelligence
TEST_TARGET := $(BUILD_DIR)/test_defect_intelligence
APP_SOURCES := src/main.cpp src/internal_utils.cpp src/defect_parsing.cpp src/sample_report.cpp src/git_scanner.cpp src/insight_summary.cpp src/json_serialization.cpp src/http_server.cpp
TEST_SOURCES := tests/test_core.cpp src/internal_utils.cpp src/defect_parsing.cpp src/sample_report.cpp src/git_scanner.cpp src/insight_summary.cpp src/json_serialization.cpp src/http_server.cpp

.PHONY: all test clean run-sample serve-sample

all: $(TARGET)

$(TARGET): $(APP_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(APP_SOURCES) -o $(TARGET) $(LDFLAGS)

$(TEST_TARGET): $(TEST_SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) -o $(TEST_TARGET) $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run-sample: $(TARGET)
	./$(TARGET) scan --sample

serve-sample: $(TARGET)
	./$(TARGET) serve --sample --port 8080

clean:
	rm -rf $(BUILD_DIR)
