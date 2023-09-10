CFLAGS=-Wextra -g

minimal_vulkan_compute: minimal_vulkan_compute.c
	$(CC) $(CFLAGS) -o minimal_vulkan_compute minimal_vulkan_compute.c -lvulkan

