build: clean
	clear
	g++ dedupe.cpp -o dedupe --std=c++17 -Wall -lmd -g

clean:
	-rm dedupe duplicates.txt
