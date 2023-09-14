#include <stdio.h>	// for fprintf()
#include <stdlib.h>	// for getenv()
#include <assert.h>	// for assert()
#include <string.h>	// for memset()

#include <vulkan/vulkan.h>

#define CHECK_VK(RES) \
	if (RES != VK_SUCCESS) \
	{ \
		fprintf(stderr, "VK FAIL (%d) at %s:%d\n", RES, __FILE__, __LINE__); \
		assert(RES == VK_SUCCESS); \
	}


static VkInstance inst;					// A Vulkan instance.
static VkPhysicalDevice pdev;				// A physical device.
static VkDevice devi;					// A device.
static int qfam = -1;					// queue family index.

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
	const VkResult res_alloc = vkAllocateMemory
	(
		devi,
		&mai,
		0,
		devmem
	);
	CHECK_VK(res_alloc);
}

#pragma mark Shader module

#define MAXSHADERSZ 4096
static uint32_t shader[MAXSHADERSZ];

static VkShaderModule mk_shader(const char* spirv_fname)
{
	FILE* f = fopen(spirv_fname, "rb");
	if (!f)
		fprintf(stderr,"Failed to open %s for reading.\n", spirv_fname);
	assert(f);
	const uint16_t shader_sz = (uint16_t) fread(shader, sizeof(uint32_t), MAXSHADERSZ, f);
	assert(shader_sz);

	// Create the shader module
	VkShaderModuleCreateInfo smci =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		0,
		0,
		shader_sz * sizeof(uint32_t),
		shader
	};
	VkShaderModule shader_module;
	const VkResult res_csm = vkCreateShaderModule(devi, &smci, 0, &shader_module);
	CHECK_VK(res_csm);
	return shader_module;
}

#pragma mark Device selection

static void pick_device(void)
{
	// Check instance layers
	uint32_t layerCount=16;
	VkLayerProperties layerProps[layerCount];
	const VkResult res_eilp = vkEnumerateInstanceLayerProperties(&layerCount, layerProps);
	CHECK_VK(res_eilp);

	int foundLayer0 = 0;
	int foundLayer1 = 0;
	for (uint32_t i=0; i<layerCount; ++i)
	{
		VkLayerProperties prop = layerProps[i];
		//fprintf(stderr,"layer: %s\n", prop.layerName);
		if (strcmp("VK_LAYER_LUNARG_standard_validation", prop.layerName) == 0)
		{
			foundLayer0 = 1;
			break;
		}
		if (strcmp("VK_LAYER_KHRONOS_validation", prop.layerName) == 0) 
		{
			foundLayer1 = 1;
			break;
		}
	}
	const char* layerName = foundLayer1 ? 
		"VK_LAYER_KHRONOS_validation" :
		"VK_LAYER_LUNARG_standard_validation";

	// Check the extensions
	uint32_t extCount = 32;
	VkExtensionProperties extProps[extCount];
	const VkResult res_eisp = vkEnumerateInstanceExtensionProperties
	(
	 	0,		// layer to retrieve extensions from
		&extCount,
		extProps
	);
	CHECK_VK(res_eisp);
	int foundExt = 0;
	for (uint32_t i=0; i<extCount; ++i)
	{
		const char* ename = extProps[i].extensionName;
		//fprintf(stderr,"Extension %s\n", ename);
		if (!strcmp(ename, "VK_EXT_debug_report"))
			foundExt = 1;
	}
	assert(foundExt);
	const char* extName = "VK_EXT_debug_report";

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
		0,			// next
		0,			// flags
		&ai,			// application info
		1,			// enabled layer count
		&layerName,		// enabled layer names
		1,			// enabled extension count
		&extName		// enabled extension names
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
	if (!dev_count) exit(2);
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
	for (uint32_t fa=0; fa<fam_count; ++fa)
		if (famprops[fa].queueFlags & VK_QUEUE_COMPUTE_BIT )
			if (famprops[fa].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				qfam = fa;
				break;
			}
	assert(qfam>=0);

	// Create a device
	const float queue_prio = 1.0f;
	const VkDeviceQueueCreateInfo dqci =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		0,					// next
		0,					// flags
		qfam,					// family idx
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
}


#pragma mark memory types

static void list_memory_types(void)
{
	// Examine the memory types.
	vkGetPhysicalDeviceMemoryProperties(pdev, &memprops);
	mtcnt = memprops.memoryTypeCount;
	mhcnt = memprops.memoryHeapCount;
	fprintf(stderr, "%d mem types (%d mem heaps)\n", mtcnt, mhcnt);
	for (uint32_t mt=0; mt<mtcnt; ++mt)
	{
		const uint32_t hidx = memprops.memoryTypes[mt].heapIndex;
		const VkMemoryPropertyFlags fl = memprops.memoryTypes[mt].propertyFlags;
		const VkDeviceSize sz = memprops.memoryHeaps[hidx].size;
		const VkMemoryHeapFlags mhf = memprops.memoryHeaps[hidx].flags;
		fprintf
		(
			stderr,
			"%7zu MiB of %s memory [ %s%s%s%s]\n",
			sz / (1024*1024),
			mhf & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "local" : "non-local",
			fl & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ? "device-local " : "",
			fl & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? "host-visible " : "",
			fl & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ? "host-coherent " : "",
			fl & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ? "host-cached " : ""
		);
	}
}

#pragma mark Main

int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	pick_device();

	list_memory_types();

	// Create a buffer for constant data
	const VkDeviceSize bufsz = 12*1024;
	VkBuffer bufsrc;
	VkDeviceMemory memsrc;
	mk_buffer
	(
		//VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		bufsz,
		&bufsrc,
		&memsrc
	);
	// Create a buffer for dest data
	VkBuffer bufdst;
	VkDeviceMemory memdst;
	mk_buffer
	(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		bufsz,
		&bufdst,
		&memdst
	);

	// Map the src memory
	void* datasrc = 0;
	const VkResult resmap0 = vkMapMemory
	(
		devi,
		memsrc,
		0,
		bufsz,
		0,
		(void**) &datasrc
	);
	CHECK_VK(resmap0);


	// Write the data
	memset(datasrc, 0x55, bufsz);

	// Flush it
	const VkMappedMemoryRange rng =
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		0,
		memsrc,
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
	vkUnmapMemory(devi, memsrc);

	// Bind src buf
	const VkDeviceSize offset = 0;
	const VkResult resbind0 = vkBindBufferMemory
	(
		devi,
		bufsrc,
		memsrc,
		offset
	);
	CHECK_VK(resbind0);
	// Bind dst buf
	const VkResult resbind1 = vkBindBufferMemory
	(
		devi,
		bufdst,
		memdst,
		offset
	);
	CHECK_VK(resbind1);

	// Make a shader module
	VkShaderModule shader_module = mk_shader("foo.spirv");
	//VkShaderModule shader_module = mk_shader2();

	// Make a descriptor set
	const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = 
	{
		{
			0,					// binding number
#if 1
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// descriptor type
#else
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	// descriptor type
#endif
			1,					// descriptor count
			VK_SHADER_STAGE_COMPUTE_BIT,		// stage flags
			0					// immutable samplers
		},
		{
			1,					// binding number
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,	// descriptor type
			1,					// descriptor count
			VK_SHADER_STAGE_COMPUTE_BIT,		// stage flags
			0					// immutable samplers
		}
	};
	VkDescriptorSetLayout  descriptorSetLayout;
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		0,				// next
		0,				// flags
		2,				// bindingCount
		descriptorSetLayoutBindings	// bindings
	};
	const VkResult rescdsl = vkCreateDescriptorSetLayout(devi, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout);
	CHECK_VK(rescdsl);

	// Create pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		0,				// next
		0,				// flags
		1,				// layout count
		&descriptorSetLayout,		// layouts
		0,				// pushConstantRangeCount
		0				// pushConstantRanges
	};
	VkPipelineLayout pipelineLayout;
	const VkResult rescpl = vkCreatePipelineLayout(devi, &pipelineLayoutCreateInfo, 0, &pipelineLayout);
	CHECK_VK(rescpl);
	const VkPipelineShaderStageCreateInfo pssci =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		0,				// next
		0,				// flags
		VK_SHADER_STAGE_COMPUTE_BIT,	// stage
		shader_module,			// module
		"foo",				// name of entry point
		0				// specialization info
	};
	VkComputePipelineCreateInfo computePipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		0,				// next
		0,				// flags
		pssci,				// pipeline shader stage create info
		pipelineLayout,			// layout
		0,				// basePipelineHandle
		0				// basePipelineIndex
	};
	VkPipeline pipeline;
	const VkResult res_cp = vkCreateComputePipelines
	(
		devi,
		0,				// pipeline cache
		1,				// create info count
		&computePipelineCreateInfo,
		0,				// allocator
		&pipeline
	);
	CHECK_VK(res_cp);

	// Descriptor pool
	const VkDescriptorPoolSize descriptorPoolSize = 
	{
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		2
	};
	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = 
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		0,				// next
		0,				// flags
		1,				// max sets
		1,				// pool size count
		&descriptorPoolSize		// pool sizes
	};
	VkDescriptorPool descriptorPool;
	VkResult rescdp = vkCreateDescriptorPool
	(
		devi,
		&descriptorPoolCreateInfo,
		0,				// allocator
		&descriptorPool
	);
	CHECK_VK(rescdp);

	// Descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = 
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		0,				// next
		descriptorPool,			// descriptor pool to allocate from
		1,				// descriptor set count
		&descriptorSetLayout		// descriptor set layouts
	};
	VkDescriptorSet descriptorSet;
	const VkResult resads = vkAllocateDescriptorSets(devi, &descriptorSetAllocateInfo, &descriptorSet);
	CHECK_VK(resads);

	const VkDescriptorBufferInfo dbi_src =
	{
		bufsrc,
		0,
		VK_WHOLE_SIZE
	};
	const VkDescriptorBufferInfo dbi_dst =
	{
		bufdst,
		0,
		VK_WHOLE_SIZE
	};

	VkWriteDescriptorSet dset[2] =
	{
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			0,
			descriptorSet,
			0,
			0,
			1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			0,
			&dbi_src,
			0
		},
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			0,
			descriptorSet,
			1,
			0,
			1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			0,
			&dbi_dst,
			0
		}
	};
	vkUpdateDescriptorSets(devi, 2, dset, 0, 0);

	// Command pool
	const VkCommandPoolCreateInfo commandPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		0,				// next
		0,				// flags
		qfam				// queue fam
	};
	VkCommandPool commandPool;
	const VkResult res_ccp = vkCreateCommandPool(devi, &commandPoolCreateInfo, 0, &commandPool);
	CHECK_VK(res_ccp);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		0,
		commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};

	// Command buffer
	VkCommandBuffer commandBuffer;
	const VkResult res_acc = vkAllocateCommandBuffers(devi, &commandBufferAllocateInfo, &commandBuffer);
	CHECK_VK(res_acc);

	// Record it
	VkCommandBufferBeginInfo commandBufferBeginInfo =
  	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		0,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		0
	};

	const VkResult res_bcb = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	CHECK_VK(res_bcb);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

	vkCmdDispatch(commandBuffer, bufsz / sizeof(uint32_t), 1, 1);

	const VkResult res_ecb = vkEndCommandBuffer(commandBuffer);
	CHECK_VK(res_ecb);

	VkQueue queue;
	vkGetDeviceQueue(devi, qfam, 0, &queue);

	VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		0,
		0,
		0,
		0,
		1,
		&commandBuffer,
		0,
		0
	};
	const VkResult res_qs = vkQueueSubmit(queue, 1, &submitInfo, 0);
	CHECK_VK(res_qs);

	const VkResult res_qwi = vkQueueWaitIdle(queue);
	CHECK_VK(res_qwi);

	// Map the dst memory
	uint32_t* datadst = 0;
	const VkResult resmap1 = vkMapMemory
	(
		devi,
		memdst,
		0,
		bufsz,
		0,
		(void**) &datadst
	);
	CHECK_VK(resmap1);

	fprintf(stderr, "Checking results...\n");
	for (uint32_t i=0; i<bufsz/4; ++i)
		assert(datadst[i] == 0xaaaaaaaa);
	fprintf(stderr, "Results are correct.\n");

	vkUnmapMemory(devi, memdst);

	return 0;
}

