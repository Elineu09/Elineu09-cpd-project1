sudoku: sudoku-serial.o 
	gcc -o sudoku sudoku-serial.o

sudoku-serial.o:
	gcc -g -c sudoku-serial.c

clean: 
	rm -f *.o sudoku

