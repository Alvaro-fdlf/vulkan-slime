PKGFLAGS = `pkg-config --cflags --libs libdrm xcb-randr xcb vulkan`

output: slime.o dumbBuffers.o vulkanSetup.o
	gcc $(PKGFLAGS) -O3 -flto -march=native slime.o dumbBuffers.o vulkanSetup.o -o output -lm

slime.o: slime.c
	gcc $(PKGFLAGS) -O3 -flto -march=native -c slime.c

dumbBuffers.o: dumbBuffers.c dumbBuffers.h
	gcc $(PKGFLAGS) -O3 -flto -march=native -c dumbBuffers.c

vulkanSetup.o: vulkanSetup.c vulkanSetup.h
	gcc $(PKGFLAGS) -O3 -flto -march=native -c vulkanSetup.c

clean:
	rm *.o output
