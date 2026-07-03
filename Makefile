CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC := src/types.cpp src/assembler.cpp src/simulator.cpp src/main.cpp
OBJ := $(SRC:.cpp=.o)
BIN := rvsim

.PHONY: all clean run test

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) *.csv

# Example: make run FILE=benchmarks/fibonacci.s
run: $(BIN)
	./$(BIN) $(FILE) --diagram --trace trace.csv

test: $(BIN)
	@for f in benchmarks/*.s; do \
		echo "--- $$f ---"; \
		./$(BIN) $$f || exit 1; \
	done
