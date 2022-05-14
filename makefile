build: clean
	clear
	g++ dedup.cpp -o dedup --std=c++17 -Wall -lmd -g

clean:
	-rm dedup duplicates.txt
