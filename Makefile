CLSPV = ${HOME}/src/clspv/build/bin/clspv

CFLAGS = -Wextra -g

minimal_vulkan_compute: minimal_vulkan_compute.c
	$(CC) $(CFLAGS) -o minimal_vulkan_compute minimal_vulkan_compute.c -lvulkan

foo.spirv: foo.cl
	$(CLSPV) -o foo.spirv foo.cl

run: minimal_vulkan_compute foo.spirv
	./minimal_vulkan_compute

