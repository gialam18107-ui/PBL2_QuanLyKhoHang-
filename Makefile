CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -MMD -MP

BUILD_DIR = build

CORE_SRCS = $(wildcard database/*.cpp) $(wildcard schema/*.cpp) $(wildcard data_io/*.cpp)
CORE_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CORE_SRCS))

BUILD_DIRS = $(BUILD_DIR) $(BUILD_DIR)/database $(BUILD_DIR)/schema $(BUILD_DIR)/data_io $(BUILD_DIR)/generator

GENERATOR_SRCS = $(wildcard generator/*.cpp)
GENERATOR_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(GENERATOR_SRCS))

all: test_io.exe generator.exe

# Compile source files into $(BUILD_DIR)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create directories as needed
$(BUILD_DIRS):
	powershell -Command "New-Item -ItemType Directory -Force '$@' | Out-Null"

test_io.exe: $(BUILD_DIR)/test_io.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

generator.exe: $(GENERATOR_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

app.exe: $(BUILD_DIR)/main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

main.exe: $(BUILD_DIR)/main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

guiding.exe: $(BUILD_DIR)/guiding.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Pattern to build any <name>.exe from $(BUILD_DIR)/<name>.o + CORE_OBJS
%.exe: $(BUILD_DIR)/%.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Run data crawler to fetch products and export all CSV files
crawl:
	python -m data_crawler.main --output-dir data --validate

-include $(CORE_OBJS:.o=.d) $(wildcard $(BUILD_DIR)/*.d)

clean:
	powershell -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }; Remove-Item -Force -ErrorAction SilentlyContinue *.exe, database/*.o, database/*.d, schema/*.o, schema/*.d, data_io/*.o, data_io/*.d, *.o, *.d"

.PHONY: all clean crawl


