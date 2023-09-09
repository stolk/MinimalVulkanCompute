
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#define CHECK_VK(RES) \
	if (RES != VK_SUCCESS) \
	{ \
		fprintf(stderr, "VK FAIL at %s:%d\n", __FILE__, __LINE__); \
		assert(RES == VK_SUCCESS); \
	}


static VkInstance inst;
static VkPhysicalDevice pdev;
static VkDevice devi;

int main(int argc, char* argv[])
{
	// Get an instance
	const VkApplicationInfo ai =
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		0,
		"vkt",
		0,
		"",
		0,
		VK_MAKE_VERSION(1,3,238)
	};
	const VkInstanceCreateInfo ici =
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		0,
		0,
		&ai,
		0,
		0,
		0,
		0
	};
	const VkResult res_ci = vkCreateInstance
	(
		&ici,
		0,
		&inst
	);
	CHECK_VK(res_ci);

	// Enumerate devices, and pick one.
	uint32_t dev_count = 64;
	VkPhysicalDevice devices[dev_count];
	VkPhysicalDeviceProperties devprops[dev_count];
	const VkResult res_enum = vkEnumeratePhysicalDevices(inst, &dev_count, devices);
	CHECK_VK(res_enum);
	fprintf(stderr, "Found %d physical devices.\n", dev_count);
	if (!dev_count) return -1;
	const char* devtypenames[] =
	{
		"OTHER",
		"iGPU",
		"dGPU",
		"vGPU",
		"CPU",
		0,
	};
	int num_igpu = 0;
	int num_dgpu = 0;
	for (int dnr=0; dnr<dev_count; ++dnr)
	{
		VkPhysicalDevice* device = devices + dnr;
		vkGetPhysicalDeviceProperties(devices[dnr], devprops+dnr);
		num_igpu += (devprops[dnr].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
		num_dgpu += (devprops[dnr].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
		const char* tnam = devtypenames[devprops[dnr].deviceType];
		fprintf(stderr,"%04x:%04x %s %s\n", devprops[dnr].vendorID, devprops[dnr].deviceID, tnam, devprops[dnr].deviceName);
	}

	int selnr = -1;
	if (num_dgpu)
		for (int i=0; i<dev_count; ++i)
			if (devprops[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { selnr=i; break; }
	if (num_igpu && selnr<0)
		for (int i=0; i<dev_count; ++i)
			if (devprops[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) { selnr=i; break; }
	if (selnr<0)
		selnr = 0;
	pdev = devices[selnr];

	// Find queue fam.
	uint32_t fam_count = 16;
	VkQueueFamilyProperties famprops[fam_count];
	vkGetPhysicalDeviceQueueFamilyProperties(pdev, &fam_count, famprops);
	int fam = -1;
	for (int fa=0; fa<fam_count; ++fa)
		if (famprops[fa].queueFlags & VK_QUEUE_COMPUTE_BIT )
			if (famprops[fa].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				fam = fa;
				break;
			}
	assert(fam>=0);

	// Create a device
	const float queue_prio = 1.0f;
	const VkDeviceQueueCreateInfo dqci =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		0,					// next
		0,					// flags
		fam,					// family idx
		1,					// queue count
		&queue_prio				// priority 0..1
	};
	const VkDeviceCreateInfo dci =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		0,					// next
		0,					// flags
		1,					// dqci count
		&dqci,					// dqci
		0,					// layer count
		0,					// layers
		0,					// extension count
		0,					// extensions
		0					// features
	};
	const VkResult res_cd = vkCreateDevice
	(
		pdev,
		&dci,
		0,					// allocator
		&devi
	);
	CHECK_VK(res_cd);

	// Examine the memory types.
	VkPhysicalDeviceMemoryProperties memprops;
	vkGetPhysicalDeviceMemoryProperties(pdev, &memprops);
	const uint32_t mtcnt = memprops.memoryTypeCount;
	const uint32_t mhcnt = memprops.memoryHeapCount;
	fprintf(stderr, "%d mem types. %d mem heaps.\n", mtcnt, mhcnt);
	for (int mt=0; mt<mtcnt; ++mt)
	{
		const uint32_t hidx = memprops.memoryTypes[mt].heapIndex;
		const VkMemoryPropertyFlags fl = memprops.memoryTypes[mt].propertyFlags;
		const VkDeviceSize sz = memprops.memoryHeaps[hidx].size;
		const VkMemoryHeapFlags mhf = memprops.memoryHeaps[hidx].flags;
		fprintf
		(
			stderr,
			"%zu bytes of %s memory [ %s%s%s%s]\n",
			sz,
			mhf & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "local" : "non-local",
			fl & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ? "device-local " : "",
			fl & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? "host-visible " : "",
			fl & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ? "host-coherent " : "",
			fl & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ? "host-cached " : ""
		);
	}

	return 0;
}

