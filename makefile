INCLUDE=./include
BIN=./bin
SRC=./src

main: main.cpp onair
	g++ main.cpp $(BIN)/onair.o -O2 -o main -lasound

onair: $(INCLUDE)/onair.h $(SRC)/onair.c
	g++ $(SRC)/onair.c -o $(BIN)/onair.o -O2 -c -lasound