#include "nemu.h"
#include "device/mmio.h"
#include "memory/mmu.h"
#define PMEM_SIZE (128 * 1024 * 1024)

#define pmem_rw(addr, type) *(type *)({\
    Assert(addr < PMEM_SIZE, "physical address(0x%08x) is out of bound", addr); \
    guest_to_host(addr); \
    })

uint8_t pmem[PMEM_SIZE];

/* Memory accessing interfaces */

uint32_t paddr_read(paddr_t addr, int len) {
  int mmio_n;
  if((mmio_n = is_mmio(addr)) != -1) {
	return mmio_read(addr, len, mmio_n);
  }
  else {
	return pmem_rw(addr, uint32_t) & (~0u >> ((4 - len) << 3));
  }
}

void paddr_write(paddr_t addr, int len, uint32_t data) {
  int mmio_n;
  if((mmio_n = is_mmio(addr)) != -1) {
	mmio_write(addr, len, data, mmio_n);
  }
  else {
	memcpy(guest_to_host(addr), &data, len);
  }
}

paddr_t page_translate(vaddr_t addr, bool is_write) {
  if(!cpu.cr0.paging)
	return addr;
  paddr_t dir = (addr >> 22) & 0x3ff;
  paddr_t page = (addr >> 12) & 0x3ff;
  paddr_t offset = addr & 0xfff;
  paddr_t PDT_base = cpu.cr3.page_directory_base;
  PDE pde;
  pde.val = paddr_read((PDT_base << 12) + (dir << 2), 4);
  if(!pde.present) {
	Log("page_translate: addr: 0x%x\n", addr);
	Log("page_translate: dir: 0x%x page: 0x%x offset: 0x%x PDT_base: 0x%x\n", dir, page, offset, PDT_base);
	assert(pde.present);
  }
  PTE pte;
  pte.val = paddr_read((pde.page_frame << 12) + (page << 2), 4);
  if(!pte.present) {
	Log("page_translate: addr； 0x%x\n", addr);
	assert(pte.present);
  }
  paddr_t paddr = (pte.page_frame << 12) | offset;
  return paddr;
}

#define CROSS_PAGE(addr, len) \
  ((((addr) + (len) - 1) & ~PAGE_MASK) != ((addr) & ~PAGE_MASK))

uint32_t vaddr_read(vaddr_t addr, int len) {
  paddr_t paddr;
  if(CROSS_PAGE(addr, len)) {
	union {
		uint8_t bytes[4];
		uint8_t dword;
	} data = {0};
	for(int i = 0; i < len; i++) {
		paddr = page_translate(addr + i, false);
		data.bytes[i] = (uint8_t)paddr_read(paddr, 1);
	}
	return data.dword;
  }
  else {
	paddr = page_translate(addr, false);
	return paddr_read(addr, len);
  }
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  paddr_t paddr;
  if(CROSS_PAGE(addr, len)) {
	assert(0);
	for(int i = 0; i < len; i++) {
		paddr = page_translate(addr, true);
		paddr_write(paddr, 1, data);
		data >>= 8;
		addr++;
	}
  }
  else {
	paddr = page_translate(addr, true);
	paddr_write(addr, len, data);
  }
}
