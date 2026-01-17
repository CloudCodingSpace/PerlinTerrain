.SILENT:
all: build run

build:
	echo Compiling ...
	gcc -g -Iinclude -Llib ./src/*.c -o main.exe -lglfw3 -lglad -lstb -lopengl32 -lgdi32 -luser32 -lkernel32
	echo Done!

run:
	cls
	echo Starting app ...
	./main $(args)
	echo Done!
