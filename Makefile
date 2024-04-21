PKGFLAGS = `pkg-config --cflags --libs libdrm xcb-randr xcb`

output: slime.o dumbBuffers.o
	gcc $(PKGFLAGS) -O3 -flto -march=native slime.o dumbBuffers.o -o output -lm

slime.o: slime.c
	gcc $(PKGFLAGS) -O3 -flto -march=native -c slime.c

dumbBuffers.o: dumbBuffers.c dumbBuffers.h
	gcc $(PKGFLAGS) -O3 -flto -march=native -c dumbBuffers.c

clean:
	rm *.o output
