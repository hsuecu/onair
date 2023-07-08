INCLUDE=./include
BIN=./bin
SRC=./src

main: main.cpp onair
	g++ main.cpp $(BIN)/onair.o -O2 -o main -lasound

onair: $(INCLUDE)/onair.h $(SRC)/onair.cpp
	g++ $(SRC)/onair.cpp -o $(BIN)/onair.o -O2 -c -lasound