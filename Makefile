WGSZ = 256

CLSPV = ${HOME}/src/clspv/build/bin/clspv

CLSPVFLAGS = --constant-args-ubo --max-ubo-size=65536 --fp16 --uniform-workgroup-size -DWGSZ=$(WGSZ)

CFLAGS = -Wextra -g -DWGSZ=$(WGSZ)


minimal_vulkan_compute: minimal_vulkan_compute.c
	$(CC) $(CFLAGS) -o minimal_vulkan_compute minimal_vulkan_compute.c -lvulkan

foo.spirv: foo.cl
	$(CLSPV) $(CLSPVFLAGS) -o foo.spirv foo.cl
	-spirv-dis foo.spirv

run: minimal_vulkan_compute foo.spirv
	./minimal_vulkan_compute

