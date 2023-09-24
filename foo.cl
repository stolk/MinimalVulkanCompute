#define uint32_t	uint

__kernel
void foo
(
	uint32_t msk,
	__global const uint32_t* __restrict__ src,
	__global uint32_t* __restrict__ dst
)
{
	const uint32_t pindex = get_global_id(0);
	uint32_t s = src[pindex];
	dst[pindex] = s ^ msk;
}

