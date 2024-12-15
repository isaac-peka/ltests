
#define KM_VMAP_IOCTL_MAP 0xd3adb33f
#define KM_VMAP_IOCTL_UNMAP 0xc0a1e5ce
#define KM_VMAP_IOCTL_FORK 0xba5eba11
#define KM_VMAP_IOCTL_UNFORK 0xb01dface

struct km_vmap_user_map {
	unsigned long long addr;
	unsigned long long npages;
};

struct km_vmap_user_unmap {
	unsigned long long addr;
};

struct km_vmap_user_fork {
	unsigned long long stack;
	unsigned long long scs;
};
