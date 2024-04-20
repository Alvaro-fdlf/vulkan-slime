PKGFLAGS = `pkg-config --cflags --libs libdrm xcb-randr xcb`

output: slime.o dumbBuffers.o
	gcc $(PKGFLAGS) -O3 slime.o dumbBuffers.o -o output -lm

slime.o: slime.c
	gcc $(PKGFLAGS) -O3 -c slime.c

dumbBuffers.o: dumbBuffers.c dumbBuffers.h
	gcc $(PKGFLAGS) -O3 -c dumbBuffers.c

clean:
	rm *.o output
