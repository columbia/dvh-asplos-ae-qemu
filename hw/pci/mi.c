#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/mi.h"
#include "hw/pci/pci.h"
#include "qemu/range.h"
#include "qapi/error.h"
#include "trace.h"
#include "sysemu/sysemu.h"

#define PCI_CAP_MI_SIZEOF 8
#define PCI_MI_DEV_CTL 2
#define PCI_MI_LOG_CTL 3
static int migration_present(PCIDevice *dev)
{
    return dev->cap_present & QEMU_PCI_CAP_MI;
}

static void restore_device_state(PCIDevice *dev) {
    /* first we stop the device.
     * vmstate (the first param) matters, but the second one doesn't for virtio*/
    vm_state_notify_one_pci(1, RUN_STATE_RUNNING, (void *)dev);

    /* TODO: restore device state */
}

static void save_device_state(PCIDevice *dev) {
    /* first we stop the device.
     * vmstate (the first param) matters, but the second one doesn't for virtio*/
    vm_state_notify_one_pci(0, RUN_STATE_PAUSED, (void *)dev);

    /* TODO: save device state */
}

void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len)

{
    int offset;

    if (!migration_present(dev) ||
        !ranges_overlap(addr, len, dev->migration_cap, PCI_CAP_MI_SIZEOF)) {
        return;
    }

    offset = addr - dev->migration_cap;
    switch (offset) {
        case PCI_MI_DEV_CTL:
            assert(len == 1);
            if (val == 0)
                save_device_state(dev);
            else
                restore_device_state(dev);
            break;

        default:
            printf("offset 0x%x is not handled\n", offset);
            ;
    }
}

void migration_cap_init(PCIDevice *dev, Error **errp)
{

    int config_offset;
    uint8_t cap_size = PCI_CAP_MI_SIZEOF;

    dev->cap_present |= QEMU_PCI_CAP_MI;

    config_offset = pci_add_capability(dev, PCI_CAP_ID_MI, 0,
                                        cap_size, errp);

    if (config_offset < 0) {
        printf("error config_offset is %d\n", config_offset);
        return;
    }

    dev->migration_cap = config_offset;

    /* Write dummy data for test*/
    memset(dev->config + config_offset + 4, 0xbe, 4);
}
