#include <stdio.h>	// for fprintf()
#include <stdlib.h>	// for getenv()
#include <assert.h>	// for assert()
#include <string.h>	// for memset()

#include <vulkan/vulkan.h>

#define CHECK_VK(RES) \
	if (RES != VK_SUCCESS) \
	{ \
		fprintf(stderr, "VK FAIL at %s:%d\n", __FILE__, __LINE__); \
		assert(RES == VK_SUCCESS); \
	}


static VkInstance inst;					// A Vulkan instance.
static VkPhysicalDevice pdev;				// A physical device.
static VkDevice devi;					// A device.

static uint32_t mtcnt;					// Memory type count
static uint32_t mhcnt;					// Memory heap count
static VkPhysicalDeviceMemoryProperties memprops;	// Properties for all memory types

#pragma mark Buffer creation

// Create a buffer for specified usage, and with specified properties.
void mk_buffer
(
	VkBufferUsageFlags usageFlags,			// How to use buffer?
	VkMemoryPropertyFlags propFlags,		// Local? Host Visible? Cached? etc.
	VkDeviceSize sz,				// Size of the buffer.
	VkBuffer* buff,					// Out: buffer
	VkDeviceMemory* devmem				// Out: memory
)
{
	// Create the buffer.
	const VkBufferCreateFlags createFlags = 0;
	const VkBufferCreateInfo bci =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		0,					// pNext
		createFlags,
		sz,
		usageFlags,
		VK_SHARING_MODE_EXCLUSIVE,		// only one queue will use it.
		0,					// queue family index count.
		0					// queue family indices.
	};
	const VkResult res_crbuf = vkCreateBuffer
	(
		devi,
		&bci,
		0,
		buff
	);
	CHECK_VK(res_crbuf);

	// Check mem requirements for it.
	VkMemoryRequirements memreqs;
	vkGetBufferMemoryRequirements(devi, *buff, &memreqs);
	fprintf
	(
		stderr,
		"reqs: size=%lu align=%lu memtp=0x%x\n",
		memreqs.size, memreqs.alignment, memreqs.memoryTypeBits
	);

	// Pick a buffer matching the props
	int tp = -1;
	for (uint32_t mt=0; mt<mtcnt; ++mt)
		if ((memprops.memoryTypes[mt].propertyFlags & propFlags) == propFlags) { tp = mt; break; }
	if (tp<0)
	{
		fprintf(stderr, "Cannot find a memory type that is both device-local and host-visible.\n");
		assert(tp>=0);
	}
	fprintf(stderr, "Using memory type index %d\n", tp);

	// TODO: test against mem reqs.

	// Allocate the memory
	const VkMemoryAllocateInfo mai =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		0,
		memreqs.size,
		tp
	};
	const VkResult resalloc = vkAllocateMemory
	(
		devi,
		&mai,
		0,
		devmem
	);
	CHECK_VK(resalloc);
}


#pragma mark Main

int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;
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
	int num_cpu  = 0;
	for (uint32_t dnr=0; dnr<dev_count; ++dnr)
	{
		VkPhysicalDevice* device = devices + dnr;
		vkGetPhysicalDeviceProperties(devices[dnr], devprops+dnr);
		num_igpu += (devprops[dnr].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
		num_dgpu += (devprops[dnr].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
		num_cpu  += (devprops[dnr].deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
		const char* tnam = devtypenames[devprops[dnr].deviceType];
		fprintf(stderr,"%04x:%04x %s %s\n", devprops[dnr].vendorID, devprops[dnr].deviceID, tnam, devprops[dnr].deviceName);
	}

	int selnr = -1;
	const char* prefer_dgpu = getenv("MVK_PREFER_DGPU");
	const char* prefer_igpu = getenv("MVK_PREFER_IGPU");
	const char* prefer_cpu  = getenv("MVK_PREFER_CPU");
	const int skip_dgpu = (num_igpu && prefer_igpu) || (num_cpu  && prefer_cpu );
	const int skip_igpu = (num_dgpu && prefer_dgpu) || (num_cpu  && prefer_cpu );
	const int skip_cpu  = (num_igpu && prefer_igpu) || (num_dgpu && prefer_dgpu);
	if (num_dgpu && !skip_dgpu)
		for (uint32_t i=0; i<dev_count; ++i)
			if (devprops[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { selnr=i; break; }
	if (num_igpu && !skip_igpu && selnr<0)
		for (uint32_t i=0; i<dev_count; ++i)
			if (devprops[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) { selnr=i; break; }
	if (num_cpu && !skip_cpu && selnr<0)
		for (uint32_t i=0; i<dev_count; ++i)
			if (devprops[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) { selnr=i; break; }
	if (selnr<0)
		selnr = 0;
	fprintf(stderr, "Using %s\n", devprops[selnr].deviceName);
	pdev = devices[selnr];

	// Find queue fam.
	uint32_t fam_count = 16;
	VkQueueFamilyProperties famprops[fam_count];
	vkGetPhysicalDeviceQueueFamilyProperties(pdev, &fam_count, famprops);
	int fam = -1;
	for (uint32_t fa=0; fa<fam_count; ++fa)
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
	vkGetPhysicalDeviceMemoryProperties(pdev, &memprops);
	mtcnt = memprops.memoryTypeCount;
	mhcnt = memprops.memoryHeapCount;
	fprintf(stderr, "%d mem types. %d mem heaps.\n", mtcnt, mhcnt);
	for (uint32_t mt=0; mt<mtcnt; ++mt)
	{
		const uint32_t hidx = memprops.memoryTypes[mt].heapIndex;
		const VkMemoryPropertyFlags fl = memprops.memoryTypes[mt].propertyFlags;
		const VkDeviceSize sz = memprops.memoryHeaps[hidx].size;
		const VkMemoryHeapFlags mhf = memprops.memoryHeaps[hidx].flags;
		fprintf
		(
			stderr,
			"%zu MiB of %s memory [ %s%s%s%s]\n",
			sz / (1024*1024),
			mhf & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "local" : "non-local",
			fl & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ? "device-local " : "",
			fl & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? "host-visible " : "",
			fl & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ? "host-coherent " : "",
			fl & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ? "host-cached " : ""
		);
	}

	// Create a buffer for constant data
	const VkBufferUsageFlags  usageFlags  = 
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const VkMemoryPropertyFlags propFlags =
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	const VkDeviceSize bufsz = 50*1024;
	VkBuffer buff;
	VkDeviceMemory devmem;
	mk_buffer
	(
		usageFlags,
		propFlags,
		bufsz,
		&buff,
		&devmem
	);

	// Map the memory
	void* data = 0;
	const VkResult resmap = vkMapMemory
	(
		devi,
		devmem,
		0,
		bufsz,
		0,
		(void**) &data
	);
	CHECK_VK(resmap);

	// Write the data
	memset(data, 0x55, bufsz);

	// Flush it
	const VkMappedMemoryRange rng =
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		0,
		devmem,
		0,			// offset
		bufsz
	};
	const VkResult resflush = vkFlushMappedMemoryRanges
	(
		devi,
		1,
		&rng
	);
	CHECK_VK(resflush);

	// Unmap it
	vkUnmapMemory(devi, devmem);

	// Bind it
	const VkDeviceSize offset = 0;
	const VkResult resbind = vkBindBufferMemory
	(
		devi,
		buff,
		devmem,
		offset
	);
	CHECK_VK(resbind);

	return 0;
}

