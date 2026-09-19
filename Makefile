CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -MMD -MP

CORE_SRCS = $(wildcard database/*.cpp) $(wildcard schema/*.cpp) $(wildcard data_io/*.cpp)
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

all: test_io.exe

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_io.exe: test_io.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

app.exe: main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

main.exe: main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

guiding.exe: guiding.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Pattern to build any <name>.exe from <name>.o + CORE_OBJS
%.exe: %.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

-include $(CORE_OBJS:.o=.d) test_io.d main.d guiding.d

clean:
	powershell -Command "Remove-Item -Force -ErrorAction SilentlyContinue *.exe, database/*.o, database/*.d, schema/*.o, schema/*.d, data_io/*.o, data_io/*.d, *.o, *.d"

.PHONY: all clean
