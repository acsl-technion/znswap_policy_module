# ZNSwap example policy

This policy is part of the [ZNSwap project][1]

The enclosed policy chooses swap-out zone-slots based on the PID of the owner-process of the page itself.

The GC policy is a watermark based policy which picks the zones that requires the least amount of data movements for GC.

In the event the policy does not perform GC and the swap device runs out of zones, there is an "emergency" GC operation will take place on its behalf.

## Install

While running the [ZNSwap kernel][1], perform the following steps:

```
make
insmod sample_policy.ko
```



# API doc

Please include `<linux/swap.h>`

```C
/* API structs */

struct pg_i {
        u64 last_swapout_t; /* last swapped-out */
        u16 access_bit_vec; /* accessed-bit vector */
        int owner_pid;      /* pid of the owner of the page */
        u64 cgroup_id;      /* cgroup_id of the owner of the page */
};

struct zn_i {
        int zone_id;        /* zone number */
        int capacity;       /* capacity (in slots) of the zone */
        int occupied_slots; /* number of occupied slots in the zone */
        int invalid_slots;  /* number of invalid (not used) slots in the zone */
        int swap_cache_slots; /* number of slots which have a swap-cache page in the zone */
        int swap_zone_id;     /* the last swap-zone slot number */
};

struct vm_i {
        u64 vm_flags;         /* the vm_flags of the first vma of the page */
        u64 size;             /* size of the vma */
        int readahead_win_sz; /* readahead window size of the vma */
        u64 cgroup_id;        /* cgroup_id of the vma */
};

struct swap_i {
        u64 num_slots;        /* total number of swap slots the device has */
        u64 num_zns;          /* total number of zones the device has */
        u64 free_slots;       /* number of free swap slots the device has */
        u64 free_zns;         /* number of free (Empty) zones the device has */
        u8 zslot_array_sz;    /* size of the swap-zone array */
        u32 high_wmark;       /* high wmark (configured via sysfs) */
        u32 low_wmark;        /* low wmark (configured via sysfs) */
        bool gc_running;      /* is vmGC currently running in the background */
};

/* Callbacks */

/* Call this function in the module init to register a callback
 * function which will get invoked when a new zone-slot needs
 * to be chosen. u64 is the victim page's pfn. swapolicy is needed
 * for some function calls within the callback function
 */
void register_policy(int(*pol)(u64, struct swappolicy*));

/* Trigger an async GC operation on zn`
extern void rec_zn(int zn);

/* Retrieve information on a page (pfn). Pass the sp element to the fuction 
 * recieved by the callback function */
extern void pg_inf(struct pg_i* pg, u64 pfn, struct swappolicy *sp);

/* Retrieve information on the page's vm_area_struct */
extern void vm_inf(struct vm_i* vm, u64 pfn, struct swappolicy *sp);

/* Retrieve information on a specific zone */
extern void zn_inf(struct zn_i* zn, int zone);

/* Retrieve information of the zoned swap device */
extern void swap_inf(struct swap_i* swap);

/* Register a callback function for the policy, which will be called
 * when a new swap-slot needs to be chosen for a victim page */
extern void register_policy(int(*pol)(u64, struct swappolicy*));
```

[1]: https://github.com/acsl-technion/znswap_policy_module "ZNSwap example policy module and API description"
