CC=c++
CFLAGS=-lwiringPi -lwiringPiDev -std=c++11
DEPS = stratFunc.h
OBJ = stratMain.o stratFunc.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

stratMain: 	$(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	chmod +x stratasys-cli.py
	chmod +x stratasys-cartridge.py
		
.PHONY: clean

clean:
	rm $(OBJ)
	rm stratMain.c
	rm stratFunc.c
	rm stratFunc.h
	rm makefile
		
		
		
		
		
