CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -Iinclude
SRC := src/main.cpp src/control_parser.cpp src/db.cpp src/package.cpp
BIN := kayp

.PHONY: all clean install

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

install: $(BIN)
	mkdir -p /usr/bin
	cp $(BIN) /usr/bin/$(BIN)
	chmod +x /usr/bin/$(BIN)
	@echo "kayp installed to /usr/bin/kayp"

clean:
	rm -f $(BIN) *.o
