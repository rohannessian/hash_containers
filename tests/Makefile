OS := $(shell uname -o)

EXE :=
ifeq ($(OS),Cygwin)
EXE := .exe
endif
ifeq ($(OS),Windows_NT)
EXE := .exe
endif

all: closed_linear_probing_hash_table closed_linear_probing_hash_table2 cpp98

closed_linear_probing_hash_table: closed_linear_probing_hash_table.cpp ../include/closed_linear_probing_hash_table.h ../include/common.h Makefile
	gcc -o $@ $< -I../include -std=c++11 -lstdc++ -O3 -DNDEBUG=1
	./$@$(EXE)

closed_linear_probing_hash_table2: closed_linear_probing_hash_table2.cpp ../include/closed_linear_probing_hash_table2.h ../include/common.h Makefile
	gcc -o $@ $< -I../include -std=c++11 -lstdc++ -O3 -DNDEBUG=1
	./$@$(EXE)

cpp98: cpp98.cpp ../include/closed_linear_probing_hash_table.h ../include/closed_linear_probing_hash_table2.h ../include/common.h Makefile
	gcc -o $@ $< -I../include -std=c++98 -lstdc++ -D_DEBUG=1
	./$@$(EXE)


clean:
	rm closed_linear_probing_hash_table$(EXE) closed_linear_probing_hash_table2$(EXE) cpp98$(EXE)


