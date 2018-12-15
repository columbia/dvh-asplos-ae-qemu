#ifndef QEMU_MI_H
#define QEMU_MI_H

#include "qemu-common.h"
#include "hw/pci/pci.h"

void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len);

void migration_cap_init(PCIDevice *dev);

#endif
