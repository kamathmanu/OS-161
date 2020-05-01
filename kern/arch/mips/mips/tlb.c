#include <tlb.h>

//function to evict (and replace) an entry in the TLB. This
//ensures that the kernel does not crash once the TLB is filled
//with entries. Right now the cache replacement algorithm is
//very basic - pick a random index and remove the entry in it,
//then write to that index. Eventually will want to replace it
//with an LRU algorithm.

void TLB_evict_entry(u_int32_t entryhi, u_int32_t entrylo){
	TLB_Random(entryhi, entrylo);
}
