#include <linux/module.h>
#include <linux/init.h>
#include <linux/swap.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/bitmap.h>

/*
 * Sample policy partitions apps via pid to different zones. Reclamation is
 * via watermarks. Hard-coded policies are available in mm/swapfile.c
 * get_zns_zone_slot()
 */

/* can get this from zn_inf on zone 0 */
#define MAX_SWP_ZNS 20
#define LOW_WMARK 7
#define HIGH_WMARK 10

static atomic_t last_free;
static atomic_t to_free;

/* this function may run from many threads, so when triggering GC do it safely.
 * for a thread-safe alternative, a callback can be implemented from the
 * function find_victim_zone() */
static unsigned long *free_bitmap;

static void free_zones(int cur_to_free)
{
	int i, freed = 0;

	while (true) {
		int max_invalid;
		int max_invalid_zone;

		max_invalid = 0;
		max_invalid_zone = -1;

		for (i = 0; i < MAX_SWP_ZNS; i++) {
			struct zn_i zn;
			int cur_invalid;

			zn_inf(&zn, i);

			/* zone is not full... */
			if (zn.occupied_slots < zn.capacity)
				continue;

			/* find zone with least amount of data to GC */
			cur_invalid = zn.invalid_slots + zn.swap_cache_slots;
			if (cur_invalid > max_invalid &&
					!test_bit(i, free_bitmap)) {
				max_invalid = cur_invalid;
				max_invalid_zone = i;
			}
		}

		BUG_ON(max_invalid_zone == -1);

		set_bit(max_invalid_zone, free_bitmap);
		freed++;
		if (freed == cur_to_free)
			break;
	}

	for_each_set_bit(i, free_bitmap, MAX_SWP_ZNS) {
		rec_zn(i);
		clear_bit(i, free_bitmap);
	}

	atomic_set(&to_free, 0);
}

int sample_policy(u64 pfn, struct swappolicy *sp)
{
	struct swap_i swap;
	int local_to_free, ret_slot;
	struct pg_i pg;

	swap_inf(&swap);
	pg_inf(&pg, pfn, sp);

	/* racy - but this is just for debugging, so who cares */
	if (swap.free_zns != atomic_read(&last_free)) {
		struct vm_i vm;

		atomic_set(&last_free, (unsigned long)swap.free_zns);
		pr_info("current free zones %llu\n", swap.free_zns);

		vm_inf(&vm, pfn, sp);
		pr_info("current pfn %llu, pid %d, cgroup %llu"
				"bitmap %x, vma_size %llu, zslot_array_sz %d\n",
				pfn, pg.owner_pid, pg.cgroup_id,
				pg.access_bit_vec, vm.size, swap.zslot_array_sz);
	}

	if (swap.free_zns < LOW_WMARK && !swap.gc_running) {
		int old;

		local_to_free = HIGH_WMARK - swap.free_zns;
		old = atomic_cmpxchg(&to_free, 0, local_to_free);
		if (old == 0)
			free_zones(local_to_free);
	}

	/* partition across swap-zones by pid */
	pg.owner_pid = pg.owner_pid > 0 ? pg.owner_pid : 0;
	ret_slot = (int)pg.owner_pid % (int)swap.zslot_array_sz;

	return ret_slot;
}

static int __init policy_init(void)
{
	atomic_set(&to_free, 0);
	atomic_set(&last_free, 0);

	free_bitmap = bitmap_zalloc(MAX_SWP_ZNS, GFP_KERNEL);

	pr_info("registering policy\n");
	register_policy(sample_policy);

	return 0;
}

static void __exit policy_exit(void)
{
	kfree(free_bitmap);
}

MODULE_LICENSE("Apache");
module_init(policy_init);
module_exit(policy_exit);
