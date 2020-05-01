#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <array.h>
#include <elf.h>
#include <vnode.h>

/*
* Note! If OPT_DUMBVM is set, as is the case until you start the VM
* assignment, this file is not compiled or linked or in any way
* used. The cheesy hack versions in dumbvm.c are used instead.
*/

struct coremap_entry * coremap;
u_int32_t coremap_base_page_offset;
u_int32_t total_coremap_entries;
u_int32_t num_free_coremap_entries;

int booted;

/********************************COREMAP***************************************/

void coremap_bootstrap() {

  // thread_bootstrap obviously hasn't been called yet.
  booted = 0;

  //ram_getsize first to figure out how much ram is first available
  u_int32_t lo, hi;

  int spl = splhigh();

  ram_getsize(&lo, &hi);

  // Must be page aligned.
  assert((lo & PAGE_FRAME) == lo);
  assert((hi & PAGE_FRAME) == hi);

  //Available memory equates to the difference between the hi and lo base and bounds.
  u_int32_t available_mem = (hi - lo);

  // Number of page frames in the coremap should equal number of physical pages we could have in
  // memory, which is the available memory divided by the size of a page (4K).
  u_int32_t num_page_frames = (available_mem) / PAGE_SIZE;

  // coremap_size is the number of page_frames times the size of one coremap entry.
  u_int32_t coremap_size = sizeof(coremap_entry) * num_page_frames;

  // Round up coremap_size to make sure its aligned with the size of a page (4K).
  coremap_size = ROUNDUP(coremap_size, PAGE_SIZE);

  // Must be page aligned.
  assert((coremap_size & PAGE_FRAME) == coremap_size);

  /*Reserve memory for coremap.*/
  coremap = (struct coremap_entry *)PADDR_TO_KVADDR(lo);
  lo += coremap_size;

  /*Assign global coremap parameters.*/

  //The base coremap page will now be located at the readjusted lo address, and
  //we will effectively use this as an offset for our coremap to track physical
  //frames.
  coremap_base_page_offset = lo;


  //Now, we effectively skip over the kernel pages, ex. handling pages, etc.

  available_mem = (hi - lo);
  total_coremap_entries = available_mem / PAGE_SIZE;
  num_free_coremap_entries = total_coremap_entries;

  u_int32_t i;

  for (i = 0; i < total_coremap_entries; i++) {

    /*Given our coremap base page offset, which starts at lo after allocating
    memory for coremap, we translate to page frames through using the ith index
    in our coremap and multiplying it by 4K while offestting by the base page.*/
    coremap[i].page_frame = (paddr_t)(i * PAGE_SIZE + coremap_base_page_offset);
    coremap[i].is_allocated = 0;
    coremap[i].chunk_size = 0;
    coremap[i].as = NULL;
  }

  splx(spl);
}

static paddr_t getppages(unsigned long npages)
{

  paddr_t addr;

  int spl = splhigh();

  unsigned int found_block = 0;

  if (npages > 1) {

    //Multiple pages.

    u_int32_t i;
    int free_entry_index;
    unsigned int frame_counter = 1;

    if (npages > num_free_coremap_entries) {
      panic("Can't allocate multiple frames.\n");
      splx(spl);
      return 0;
    }

    for (i = 0; i < total_coremap_entries; i++) {

      /*We are only interested in this frame if the frame next to it is available
      as well.*/
      if (!coremap[i].is_allocated && !coremap[i + 1].is_allocated) {
        frame_counter++;
      }
      else {
        /*The frame next to it is not available, so this isn't the contiguous
        block that we are looking for.*/
        frame_counter = 1;
      }
      if (frame_counter == npages) {
        /*Frames found!*/
        found_block = 1;
        free_entry_index = i;
        break;
      }
    }

    if (!found_block) {
      kprintf("Didn't find suitable block.\n");
      splx(spl);
      return 0;
    }

    num_free_coremap_entries -= npages;

    for (i = free_entry_index; i < (free_entry_index + npages); i++) {

      coremap[i].is_allocated = 1;
      coremap[i].chunk_size = npages;

      if (booted) {
        coremap[i].as = curthread->t_vmspace;
      }
    }

    addr = (paddr_t)((free_entry_index * PAGE_SIZE) + coremap_base_page_offset);

    // Must be page aligned.
    assert((addr & PAGE_FRAME) == addr);

    // Check base and bounds for this address to check if its valid.

  }

  else {

    //Single page.

    if (num_free_coremap_entries == 0) {
      panic("Can't allocate single frame.\n");
      return 0;
    }

    int free_entry_index = -1;
    u_int32_t i;

    for (i = 0; i < total_coremap_entries && !found_block; i++) {
      if (!coremap[i].is_allocated) {
        free_entry_index = i;
        found_block = 1;
      }
    }

    coremap[free_entry_index].is_allocated = 1;
    coremap[free_entry_index].chunk_size = 1;

    if (booted) {
      coremap[free_entry_index].as = curthread->t_vmspace;
    }


    addr = (paddr_t)((free_entry_index * PAGE_SIZE) + coremap_base_page_offset);
  }

//NOTE: it is possible for invalid physical address being access so need to handle this...

  if (addr == (paddr_t)(0x0)) {
    splx(spl);
    return 0;
  }

  splx(spl);
  return addr;
}

 /* Allocate/free some kernel-space virtual pages */
 vaddr_t alloc_kpages(int npages) {

   paddr_t pa;
   pa = getppages(npages);
   if (pa == 0) {
     return 0;
   }

   return PADDR_TO_KVADDR(pa);
 }

 void free_kpages(vaddr_t addr) {

   int spl = splhigh();

   paddr_t p_addr = KVADDR_TO_PADDR(addr);

   u_int32_t physical_page_number = (p_addr - coremap_base_page_offset) / PAGE_SIZE;

   if (coremap[physical_page_number].chunk_size == 1) {

     // Freeing a single page.

     coremap[physical_page_number].is_allocated = 0;
     coremap[physical_page_number].chunk_size = 0;

     if (booted) {
       coremap[physical_page_number].as = NULL;
     }

     num_free_coremap_entries++;
   }

   else if (coremap[physical_page_number].chunk_size > 1) {

     // Freeing multiple pages.

     unsigned int length = coremap[physical_page_number].chunk_size;

     unsigned int i;
     for (i = physical_page_number; i < (length + physical_page_number); i++) {

       coremap[i].is_allocated = 0;
       coremap[i].chunk_size = 0;

       if (booted) {
         coremap[i].as = NULL;
       }
     }

     num_free_coremap_entries += length;
     splx(spl);
   }

   //NOTE: Need to check for possible errors where free() will fail, need to handle this

 }

/**************************ADDRESS SPACE FUNCTIONS*****************************/

struct addrspace *
as_create(void)
{
  struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

  //initialize level 1 of our PT - set all ptrs to NULL

  int i;
  for(i = 0; i < NUMBER_OF_PAGE_TABLE_ENTRIES; i++){
    as->master_page_table[i] = NULL;
	}

  for (i=0; i < NUM_SEGMENTS; i++){
    (as->memory_segments[i]).v_base = (vaddr_t)0;
    (as->memory_segments[i]).npages = (vaddr_t)0;
    (as->memory_segments[i]).permissions = 0;
  }

  (as->as_v).as_vnode = NULL;

  for (i = 0; i < NUM_STATIC_SEGMENTS; i++) {
    (as->as_v).offset[i] = 0;
  }

  for (i = 0; i < NUM_STATIC_SEGMENTS; i++) {
    (as->as_v).filesize[i] = 0;
  }

	as->as_brk = (vaddr_t)0;
  as->as_stackptr = (vaddr_t)0;
  as->heap_max = (vaddr_t)0;

	return as;
}

void
as_destroy(struct addrspace *as)
{
  assert(as != NULL);

  int spl = splhigh();
  unsigned int i;

  /*Free up any frames associated with this address space.*/
  for (i = 0; i < total_coremap_entries; i++) {

    if (coremap[i].as == as) {
      coremap[i].is_allocated = 0;
      coremap[i].chunk_size = 0;

      if (booted) {
        coremap[i].as = NULL;
      }
    }
  }

  /*Walk through the page table starting from level 2 destoying relevant page
    table arrays.*/

  for (i = 0; i < NUMBER_OF_PAGE_TABLE_ENTRIES; i++) {

    struct page_table_entry_array * master = as->master_page_table[i];

    if (master != NULL) {

      unsigned int j;
      for (j = 0; j < NUMBER_OF_PAGE_TABLE_ENTRIES; j++) {

        struct page_table_entry * PTE = as->master_page_table[i]->second_level_page_table[j];

        if (PTE != NULL) {
          kfree(PTE);
        }
      }

      kfree(master);
    }
  }

  kfree(as);
  splx(spl);
}

void
as_activate(struct addrspace *as)
{
  int i;

	(void)as;

  //flush the TLB after the context switch

	int spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable, size_t ph_index)
{
  size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

  assert((vaddr & PAGE_FRAME) == vaddr);

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;
  assert(sz % PAGE_SIZE == 0);

	npages = sz / PAGE_SIZE;

  //define a new memory segment. For some reason ph_index starts with 1.

  (as->memory_segments[ph_index-1]).v_base = vaddr;
  (as->memory_segments[ph_index-1]).npages = npages;
  (as->memory_segments[ph_index-1]).permissions = readable | writeable | executable;

	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
  int spl = splhigh();

  struct addrspace *new;

  //create an empty address space first

	new = as_create();
	if (new==NULL) {
    splx(spl);
		return ENOMEM;
	}

  //Easy stuff first.

  new->as_brk = old->as_brk;
  new->heap_max = old->heap_max;
  new->as_stackptr = old->as_stackptr;

// //Now copy the segments info.
//
  unsigned int i;
//
  for (i = 0; i < NUM_SEGMENTS; i++){
      new->memory_segments[i].v_base = old->memory_segments[i].v_base;
      new->memory_segments[i].npages = old->memory_segments[i].npages;
      new->memory_segments[i].permissions = old->memory_segments[i].permissions;
  }

  //Page table next.

  for(i = 0; i < NUMBER_OF_PAGE_TABLE_ENTRIES; i++){

    if (old->master_page_table[i] != NULL){

      new->master_page_table[i] = kmalloc(sizeof(struct page_table_entry_array));

      int j;
      for (j = 0; j < NUMBER_OF_PAGE_TABLE_ENTRIES; j++){

        if (old->master_page_table[i]->second_level_page_table[j] != NULL){

          new->master_page_table[i]->second_level_page_table[j] = kmalloc(sizeof(struct page_table_entry));
          (new->master_page_table[i]->second_level_page_table[j])->permissions = (old->master_page_table[i]->second_level_page_table[j])->permissions;
          (new->master_page_table[i]->second_level_page_table[j])->pfn = getppages(1); //get a fresh frame
        }

        else{
          new->master_page_table[i]->second_level_page_table[j] = old->master_page_table[i]->second_level_page_table[j]; //the original points to NULL so just set this
        }
      }
    }
    else {
      //Old master table is NULL, copy it.
      new->master_page_table[i] = old->master_page_table[i];
    }
  }

  for (i = 0; i < NUM_STATIC_SEGMENTS; i++) {
    (new->as_v).filesize[i] = (old->as_v).filesize[i];
    (new->as_v).offset[i] = (old->as_v).offset[i];
  }

  // /*Inherit the vnode.*/
  // if (old->as_v.as_vnode != NULL) {
  //
  //   // VOP_INCOPEN(old->as_v.as_vnode);
  //   VOP_INCREF(old->as_v.as_vnode);
  //
  //   new->as_v.as_vnode = kmalloc(sizeof(struct vnode));
  //
	// 	if (new->as_v.as_vnode == NULL) {
  //     kfree(new->as_v.as_vnode);
	// 		return ENOMEM;
	// 	}
  //
	// 	memcpy(new->as_v.as_vnode, old->as_v.as_vnode, sizeof(struct vnode));
  // }

  // //Copy the vnode while making sure to increase the reference count.
  //
  VOP_INCOPEN((old->as_v).as_vnode);
  VOP_INCREF((old->as_v).as_vnode);
  //
  // struct vnode temp = *(old->as_v).as_vnode;
  // (new->as_v).as_vnode = &temp;

  *ret = new;

  splx(spl);
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
  //Setup heap
  (as->memory_segments[HEAP]).v_base = (as->memory_segments[DATA]).v_base + (as->memory_segments[DATA]).npages*PAGE_SIZE; //end of data = heap.
  (as->memory_segments[HEAP]).npages = (size_t)0;
  (as->memory_segments[HEAP]).permissions = PF_R | PF_W;
  as->as_brk = (as->memory_segments[HEAP]).v_base; //initial break value is the base
  as->heap_max = (as->memory_segments[HEAP]).v_base + 81290;

  //Setup stack
  (as->memory_segments[STACK]).v_base = USERSTACK;
  (as->memory_segments[STACK]).npages = JAFFVM_STACKPAGES;
  (as->memory_segments[STACK]).permissions = PF_R | PF_W;
  as->as_stackptr = USERSTACK; //initially stack points at the top of user VAs

  return 0;
}

int
as_complete_load(struct addrspace *as)
{
  //assert(); //assume the heap and stack don't overlap
  (void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	(void)as;
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	return 0;
}

/*****************************PAGE TABLE FUNCTIONS*****************************/

/*Intended to be bitwise ANDED with the fault address in question to retain the
  first 10 bits.*/
#define FIRST_LEVEL_PAGE_TABLE_INDEX_MASK 0xFFC00000

/*Intended to be bitwise ANDED with the fault address in question to retain the
  mid 10 bits.*/
#define SECOND_LEVEL_PAGE_TABLE_INDEX_MASK 0x003FF000

//similarly get the offset

#define OFFSET_MASK 0x00000FFF

#define VPN_BITS 22
#define OFFSET_BITS 12

//helper function to initialize the 2nd level of the page table.

static
struct page_table_entry_array*
page_table_L2_create(){

  struct page_table_entry_array *level2_pagetable = kmalloc(sizeof(struct page_table_entry_array));
  if (level2_pagetable == NULL){
    return NULL;
  }

  int i;
	for (i = 0; i < NUMBER_OF_PAGE_TABLE_ENTRIES; i++) {
		level2_pagetable->second_level_page_table[i] = NULL ;
	}
  return (level2_pagetable);
}

//helper fn to create a PTE

static
struct page_table_entry*
create_PTE(int permissions) {

  assert(curspl > 0); //Interrupts should still be off

  //Allocate a page for the segment on demand

  struct page_table_entry * PTE = kmalloc(sizeof(struct page_table_entry));
  if (PTE == NULL) {
    return NULL;
  }

  PTE->pfn = getppages(1); //allocate a new frame
  PTE->permissions = permissions;

  return PTE;
}

//Function to find a PTE given a VPN (already divided into the higher and lower
//bits). We return a pointer to the page table entry.

static
struct page_table_entry*
find_page_table_entry (struct addrspace *as, u_int32_t idx_L1, u_int32_t idx_L2, unsigned int operation_type, int *err)
{
    //Strategy: use the master & secondary page numbers. This allows
    //you to access the various levels of the cache. Then check the permissions
    //flag. Are we allowed to perform the operation? If yes, then return the found
    //PTE. If there is no existing such PTE, return NULL.

    struct page_table_entry* searched = NULL;

    assert(as != NULL);
    assert(as->master_page_table[idx_L1] != NULL); //at least the 1st level must be pointing
                                                  //to a valid substructure in L2.

    searched = (as->master_page_table[idx_L1])->second_level_page_table[idx_L2];

    // if(searched != NULL){
    //   if (operation_type != searched->permissions){
    //     *err = EFAULT; //an invalid operation
    //   }
    // }

    // if (searched == NULL) {
    //   *err = EFAULT;
    // }
    return (searched);
}

static
int
alloc_segment_on_demand(vaddr_t faultaddress, unsigned int region_no, int permissions){

  assert(curspl > 0); //Interrupts should still be off

  size_t filesize;
  off_t offset;
  off_t region_offset;
  struct vnode *as_vnode;
  int is_executable;

  as_vnode = (curthread->t_vmspace->as_v).as_vnode;

  // assert (as_vnode->vn_refcount > 0);
  // assert(as_vnode->vn_ops != (char *)0xdeadbeef);


  region_offset = (curthread->t_vmspace->as_v).offset[region_no];

  offset = region_offset + (faultaddress) - (curthread->t_vmspace->memory_segments[region_no]).v_base;

  filesize = (curthread->t_vmspace->as_v).filesize[region_no];

  is_executable = permissions & PF_X;

  int result = load_segment(as_vnode, offset, faultaddress, PAGE_SIZE, filesize, is_executable);

  return result;

}

/*******************************VM_FAULT***************************************/

//helper function to determine if the fault address lies within a segment

static
int
check_address_region(u_int32_t vbase, u_int32_t vtop, vaddr_t faultaddress)
{

  assert(vtop >= vbase);

  if(faultaddress >= vbase && faultaddress < vtop){
    return 1;
  }
  else return 0;
}

static
int
TLB_page_fault_handler(int faulttype, vaddr_t faultaddress, int permissions, u_int32_t region_no)
{

/*******************************BASIC SET-UP***********************************/
  /* Interrupts should already be off. */
	assert(curspl > 0);

  int err = 0;

  vaddr_t temp_vaddr = faultaddress;

  unsigned int loaded = 0;

  switch (faulttype) {
      case VM_FAULT_READONLY:
          /* We always create pages read-write, so we can't get this */
          panic("Invalid: fault on page read");

      case VM_FAULT_READ:
      case VM_FAULT_WRITE:
          break;
      default:
          // splx(spl);//THIS IS WRONG
          return EINVAL;
  }

/*************************DETERMINING A PAGE FAULT******************************/

  unsigned int level_1_index = (faultaddress & FIRST_LEVEL_PAGE_TABLE_INDEX_MASK) >> VPN_BITS;
  unsigned int level_2_index = (faultaddress & SECOND_LEVEL_PAGE_TABLE_INDEX_MASK) >> OFFSET_BITS;
  unsigned int offset = temp_vaddr & OFFSET_MASK;

  struct page_table_entry_array * level_2_page_table = NULL;

  if (curthread->t_vmspace->master_page_table[level_1_index] == NULL) {

    /*Second level page table does not exist yet. We need to initialize the second level first before loading on demand.*/

    level_2_page_table = page_table_L2_create();
    if (level_2_page_table == NULL){
      return (ENOMEM);
    }

    level_2_page_table->second_level_page_table[level_2_index] = create_PTE(permissions);
    if (level_2_page_table->second_level_page_table[level_2_index] == NULL){
      return (ENOMEM);
    }

    curthread->t_vmspace->master_page_table[level_1_index] = level_2_page_table;
  }

  else {

    //Search if the page table entry exists

    level_2_page_table = curthread->t_vmspace->master_page_table[level_1_index];

    struct page_table_entry *existing_page = find_page_table_entry(curthread->t_vmspace, level_1_index, level_2_index, faulttype, &err);

    if (existing_page == NULL){
        level_2_page_table->second_level_page_table[level_2_index] = create_PTE(permissions);
        if (level_2_page_table->second_level_page_table[level_2_index] == NULL){
          return (ENOMEM);
        }
    }

    else{
          loaded = 1;
    }
  }

  //If the page entry ^ was valid (non-NULL), it was just a TLB miss. Either ways
  //we need to update the TLB.

/******************************UPDATING THE TLB*********************************/

  //NOTE: for the below to work, our permissions flag must be 3 bits

  assert(curspl > 0); //Interrupts should still be off

  paddr_t paddr = ((level_2_page_table->second_level_page_table[level_2_index]->pfn)) | offset;

  // Must be page aligned.
  assert((paddr & PAGE_FRAME) == paddr);

  //Update the TLB. First traverse through, until we find an empty entry
  //Then write the latest mapping onto there.

  u_int32_t ehi, elo;
  unsigned int written = 0;
  int i;

      for (i=0; i<NUM_TLB; i++) {
    		TLB_Read(&ehi, &elo, i);
    		if (elo & TLBLO_VALID) {
    			continue;
    		}
    		ehi = faultaddress;
    		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
    		DEBUG(DB_VM, "TLB_entry: 0x%x -> 0x%x\n", faultaddress, paddr);
        assert((TLB_Probe(ehi, 0) < 0) || !(elo & TLBLO_VALID));   //check no duplicate entries
    		TLB_Write(ehi, elo, i);
    		written = 1;
        break;
    	}

      if (!written){
        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

        //TLB is full. Evict an entry and replace it with the new one
        TLB_Random(ehi, elo);
     }
      //ensure we have updated the TLB successfully. That is, the next time we
      //search for this vaddr, it should be a TLB hit. This means there should be
      //a matching entry in the TLB with the valid bit set.

      assert(TLB_Probe(ehi, 0) >= 0);
      assert(elo & TLBLO_VALID);

      //NOW IF THE SEGMENT WAS TEXT OR DATA, ACTUALLY LOAD ON DEMAND USING load_segment

      if (((region_no == TEXT) && (!loaded)) || ((region_no == DATA) && (!loaded))) {
        err = alloc_segment_on_demand(faultaddress, region_no, permissions);
      }

      /*if (region_no == STACK){
        if (!(faultaddress > HEAP_MAX && faultaddress < curthread->t_vmspace->as_stackptr)){
            return EFAULT;
        }
        else{
          curthread->t_vmspace->as_stackptr = faultaddress; //faultaddress is page aligned, so the stackptr is too
        }
      }*/
      return (err);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
  vaddr_t vbase;
  size_t bounds;
	struct addrspace *as;
  unsigned int permissions = 0;
  int err;

  int spl = splhigh();

  faultaddress &= PAGE_FRAME;

  DEBUG(DB_VM, "vm_fault(): 0x%x\n", faultaddress);

  as = curthread->t_vmspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

  //first check if faultaddress is a valid virtual address - we
  //need to do this manually by checking if we are within the bounds
  //for each individual segments.

  //we first check the non-stack static segments

  //a flag for whether we found which segment the fault address resides in
  unsigned int found_addr = 0;

  int i, result;
  for (i = 0; i < NUM_STATIC_SEGMENTS; i++){

      vbase = (as->memory_segments[i]).v_base;
      bounds = (as->memory_segments[i]).npages;
      //check vbase is page aligned
      assert((vbase & PAGE_FRAME) == vbase);

      result = check_address_region(vbase,(vbase + bounds*PAGE_SIZE), faultaddress);
      if (result){
        found_addr = 1;
        permissions = (as->memory_segments[i]).permissions; //get the permission of the region

        //the handler should return a success/error value and then u need to handle
        //errors.
        err = TLB_page_fault_handler(faulttype, faultaddress, permissions, i);
        splx(spl);
        return err;
      }
  }

  //check if it's in the heap. Return an error since that implies your stack has
  //overflown (since sbrk should be the way to allocate on the heap).

  if (!found_addr){
    int result;
    result = check_address_region((as->memory_segments[HEAP]).v_base, as->as_brk, faultaddress);
    if (result){
      found_addr = 1;
      permissions = PF_R | PF_W; //the stack is read-write
      err = TLB_page_fault_handler(faulttype, faultaddress, permissions, HEAP);
      splx(spl);
      return 0;
    }
  }
  //now check if it's in the stack.

  if (!found_addr){
    int result;
    result = check_address_region((USERSTACK - JAFFVM_STACKPAGES*PAGE_SIZE), USERSTACK, faultaddress);
    if (result){
      found_addr = 1;
      permissions = PF_R | PF_W; //the stack is read-write
      err = TLB_page_fault_handler(faulttype, faultaddress, permissions, STACK);
      splx(spl);
      return err;
    }
  }
  splx(spl);
  return (SIGSEGV); //if the fault address cannot be mapped to a valid virtual
                    //address in one of the segments, it is a segmentation fault;
}

void
vm_bootstrap(void)
{
	/* Do nothing. */
}
