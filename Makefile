all: client
	gcc -o ./build/run ./src/feeder.c -Wall

client:
	gcc -o ./build/client ./src/c_process.c -Wall

clean:
	rm -f ./build/*
	rm -f out.txt
