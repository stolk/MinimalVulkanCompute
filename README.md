# MinimalVulkanCompute

Minimal code to do vulkan compute.

# Purpose

Shows how to setup vulkan to compute something on the GPU.

# Dependencies

 * libvulkan-dev
 * vulkan-validationlayers

# Environment Variables

**MVK_PREFER_DGPU** Pick a discrete GPU over an integrated GPU.

**MVK_PREFER_IGPU** Pick an integrated GPU over a discrete GPU.

# Memory Types

## Using NVIDIA GeForce RTX 3070
```
5 mem types. 3 mem heaps.
23890 MiB of non-local memory [ ]
8192 MiB of local memory [ device-local ]
23890 MiB of non-local memory [ host-visible host-coherent ]
23890 MiB of non-local memory [ host-visible host-coherent host-cached ]
246 MiB of local memory [ device-local host-visible host-coherent ]
```

## Using AMD Radeon RX 580 Series (RADV POLARIS10)
```
7 mem types. 2 mem heaps.
4096 MiB of local memory [ device-local ]
4096 MiB of local memory [ device-local ]
15908 MiB of non-local memory [ host-visible host-coherent ]
4096 MiB of local memory [ device-local host-visible host-coherent ]
4096 MiB of local memory [ device-local host-visible host-coherent ]
15908 MiB of non-local memory [ host-visible host-coherent host-cached ]
15908 MiB of non-local memory [ host-visible host-coherent host-cached ]
```

## Using AMD Radeon Graphics (RADV RENOIR)
```
11 mem types. 2 mem heaps.
10819 MiB of local memory [ device-local ]
10819 MiB of local memory [ device-local ]
5409 MiB of non-local memory [ host-visible host-coherent ]
10819 MiB of local memory [ device-local host-visible host-coherent ]
10819 MiB of local memory [ device-local host-visible host-coherent ]
5409 MiB of non-local memory [ host-visible host-coherent host-cached ]
5409 MiB of non-local memory [ host-visible host-coherent host-cached ]
10819 MiB of local memory [ device-local ]
5409 MiB of non-local memory [ host-visible host-coherent ]
10819 MiB of local memory [ device-local host-visible host-coherent ]
5409 MiB of non-local memory [ host-visible host-coherent host-cached ]
```

## Using Intel(R) Xe Graphics (TGL GT2)
```
1 mem types. 1 mem heaps.
11759 MiB of local memory [ device-local host-visible host-coherent host-cached ]
```

# Author

Bram Stolk


