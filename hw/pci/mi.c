#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/mi.h"
#include "hw/pci/pci.h"
#include "qemu/range.h"
#include "qapi/error.h"
#include "trace.h"

static int migration_present(PCIDevice *dev)
{
    return dev->cap_present & QEMU_PCI_CAP_MI;
}

void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len)

{
    if (!migration_present(dev) ||
        !ranges_overlap(addr, len, dev->migration_cap, 4)) {
        return;
    }
    printf("migration_write\n");
}

void migration_cap_init(PCIDevice *dev, Error **errp)
{

    int config_offset;
    uint8_t cap_size = 8; /* One 16 bit reg + next cap ptr + cap id */

    dev->cap_present |= QEMU_PCI_CAP_MI;

    config_offset = pci_add_capability(dev, PCI_CAP_ID_MI, 0,
                                        cap_size, errp);

    if (config_offset < 0) {
        printf("error config_offset is %d\n", config_offset);
        return;
    }

    printf("%s config_offset is 0x%x\n", dev->name, config_offset);
    dev->migration_cap = config_offset;

    memcpy(dev->config + config_offset + PCI_CAP_FLAGS, &cap_size,
           1);


}
