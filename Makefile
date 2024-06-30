PKGFLAGS = `pkg-config --cflags --libs libdrm xcb-randr xcb vulkan`
OPTFLAGS = -O3 -flto -march=native
# can add -fsanitize=addressi to DEBUGFLAGS if not using valgrind
# also can add -Wconversion, but it may be too strict
DEBUGFLAGS = -DDEBUG -Wall -Wextra -Wshadow -Wfloat-equal -Wduplicated-cond -Wlogical-op -fsanitize=undefined -fno-sanitize-recover -g
CFLAGS = 

all: CFLAGS = $(OPTFLAGS)
all: output

debug: CFLAGS = $(DEBUGFLAGS)
debug: output

output: slime.o drmMaster.o dumbBuffers.o vulkanSetup.o
	gcc $(PKGFLAGS) $(CFLAGS) slime.o drmMaster.o dumbBuffers.o vulkanSetup.o -o output -lm

slime.o: slime.c
	gcc $(PKGFLAGS) $(CFLAGS) -c slime.c

drmMaster.o: drmMaster.c drmMaster.h
	gcc $(PKGFLAGS) $(CFLAGS) -c drmMaster.c

dumbBuffers.o: dumbBuffers.c dumbBuffers.h
	gcc $(PKGFLAGS) $(CFLAGS) -c dumbBuffers.c

vulkanSetup.o: vulkanSetup.c vulkanSetup.h compute.spv vertex.spv fragment.spv
	gcc $(PKGFLAGS) $(CFLAGS) -c vulkanSetup.c

compute.spv: compute.comp
	glslangValidator -V compute.comp -o compute.spv

vertex.spv: vertex.vert
	glslangValidator -V vertex.vert -o vertex.spv

fragment.spv: fragment.frag
	glslangValidator -V fragment.frag -o fragment.spv

clean:
	rm *.o *.spv output
