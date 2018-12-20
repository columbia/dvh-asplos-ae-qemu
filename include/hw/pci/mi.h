#ifndef QEMU_MI_H
#define QEMU_MI_H

#include "qemu-common.h"
#include "hw/pci/pci.h"

void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len);

void migration_cap_init(PCIDevice *dev, Error **errp);

int migration_cap_init_bar(struct PCIDevice *dev, MemoryRegion *cfg_bar,
                           uint8_t cfg_bar_nr, unsigned cfg_offset,
                           uint8_t cap_pos, Error **errp);

int migration_cap_init_exclusive_bar(PCIDevice *dev, uint8_t bar_nr, Error **errp);
#endif
