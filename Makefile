PKGFLAGS = `pkg-config --cflags --libs libdrm xcb-randr xcb vulkan`
OPTFLAGS = -O3 -flto -march=native
DEBUGFLAGS = -Wall -Wextra -Wshadow -Wconversion -Wfloat-equal -Wduplicated-cond -Wlogical-op -fsanitize=undefined -fno-sanitize-recover -g
# can add -fsanitize=addressi to DEBUGFLAGS if not using valgrind
CFLAGS = 

all: CFLAGS = $(OPTFLAGS)
all: output

debug: CFLAGS = $(DEBUGFLAGS)
debug: output

output: slime.o dumbBuffers.o vulkanSetup.o
	gcc $(PKGFLAGS) $(CFLAGS) slime.o dumbBuffers.o vulkanSetup.o -o output -lm

slime.o: slime.c
	gcc $(PKGFLAGS) $(CFLAGS) -c slime.c

dumbBuffers.o: dumbBuffers.c dumbBuffers.h
	gcc $(PKGFLAGS) $(CFLAGS) -c dumbBuffers.c

vulkanSetup.o: vulkanSetup.c vulkanSetup.h
	gcc $(PKGFLAGS) $(CFLAGS) -c vulkanSetup.c

clean:
	rm *.o output
