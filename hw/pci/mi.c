#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/mi.h"
#include "hw/pci/pci.h"
#include "qemu/range.h"
#include "qapi/error.h"
#include "trace.h"
#include "sysemu/sysemu.h"
#include "migration/savevm.h"
#include "migration/qemu-file.h"
#include "qemu/error-report.h"

#define PCI_CAP_MI_SIZEOF 8
#define PCI_MI_CONFIG 4
#define PCI_MI_DEV_CTL 2
#define PCI_MI_LOG_CTL 3
#define PCI_MI_LOG_BADDR 4



static int migration_present(PCIDevice *dev)
{
    return dev->cap_present & QEMU_PCI_CAP_MI;
}

static void copy_device_state(PCIDevice *dev, uint8_t *dev_state, size_t sz,
                              bool from_device)
{
    hwaddr baddr;
    AddressSpace *as;
    uint8_t *hva;
    struct migration_info *mi = (struct migration_info *)dev->migration_info;

    printf("%s starts\n", __func__);

    baddr = ((uint64_t)mi->state_baddr_hi) << 32 | mi->state_baddr_lo;
    as = pci_device_iommu_address_space(dev);

    while (sz) {
        hwaddr len = sz;

        hva = dma_memory_map(as, baddr, &len, DMA_DIRECTION_TO_DEVICE);
        printf("len: 0x%lx\n", len);

        /* TODO: check if the device state exceeds the given buffer */
        if (from_device)
            memcpy(hva, dev_state, len);
        else
            memcpy(dev_state, hva, len);

        sz -= len;
        baddr += len;
        dev_state += len;
    }
    printf("%s done\n", __func__);
}

static void restore_device_state(PCIDevice *dev) {
    /* we are about to start the device */
    vm_state_notify_one_pci(1, RUN_STATE_RUNNING, (void *)dev);

    /* TODO: restore device state */
}

static void save_device_state(PCIDevice *dev) {
    struct migration_info *mi = (struct migration_info *)dev->migration_info;
    SaveStateEntry *se;
    int dev_state_size;
    QEMUFile *f;
    int ret;

    /* 1. stop the device */
    vm_state_notify_one_pci(0, RUN_STATE_PAUSED, (void *)dev);

    /* 2. capture the device state to qemu file (temp) */
    se = qemu_savevm_get_se_opaque(dev->mi_opaque);
    if (!se) {
        printf("WARNING: can't find device in se list\n");
        return;
    } else {
        printf("Found device in se list: %s\n", dev->name);
    }

    f = create_mem_QEMUFile();
    if (!f)
        error_report("unable to create in-memory QEMU file");

    ret = qemu_savevm_save_device_state(f, se, &dev_state_size);
    if (ret) {
        printf("WARNING: virtio dev state save is failed: %d.\n", ret);
        mi->state_size = 0;
        return;
    }

    /* 3. copy from qemu file to baddr */
    copy_device_state(dev, qemu_get_dev_state(f), dev_state_size, true);

    /* 4. setup the device state size */
    mi->state_size = dev_state_size;
}

void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len)

{
    int offset;
    uint8_t *hva;
    AddressSpace *as;
    dma_addr_t pa, lenn;

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
        case PCI_MI_LOG_BADDR:
            printf("log baddr is called!\n");

            as = pci_device_iommu_address_space(dev);
            pa = 0x380000000;
            lenn = 4096;

            hva = dma_memory_map(as, pa, &lenn, DMA_DIRECTION_TO_DEVICE);
            printf("The first byte is 0x%x\n", *hva);
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

static uint64_t migration_mmio_read(void *opaque, hwaddr addr,
                                    unsigned size)
{

    PCIDevice *dev = opaque;
    uint64_t val = 0;

    switch (addr) {
        case MI_STATE_CTL:
        case MI_LOG_CTL:
        case MI_STATE_BADDR_LO:
        case MI_STATE_BADDR_HI:
        case MI_STATE_SIZE:
        case MI_LOG_SIZE:
        case MI_LOG_BADDR_LO:
        case MI_LOG_BADDR_HI:
            val = *(uint32_t *)(dev->migration_info + addr);
            break;
        default:
            printf("Reading 0x%lx from mi device memory is not defined\n", addr);
    }

    return val;
}

static void handle_state_ctl_write(PCIDevice *dev, uint32_t val)
{
    switch(val) {
        case MI_STATE_CTL_RESET:
            /* TODO: reset */
            printf("mi cap state reset - todo\n");
            /* test: search for the device in se list */
            qemu_savevm_get_se_opaque(dev->mi_opaque);
            break;
        case MI_STATE_CTL_SAVE:
            printf("mi cap save state\n");
            save_device_state(dev);
            break;
        case MI_STATE_CTL_RESTORE:
            printf("mi cap restore state\n");
            restore_device_state(dev);
            break;
        default:
            printf("Writing %d to state ctl register is not defined\n", val);
    }
}

static void read_addr_contents(PCIDevice *dev)
{
    uint8_t *hva;
    AddressSpace *as;
    dma_addr_t pa, lenn;

    as = pci_device_iommu_address_space(dev);
    pa = 0x380000000;
    lenn = 4096;

    hva = dma_memory_map(as, pa, &lenn, DMA_DIRECTION_TO_DEVICE);
    printf("The first byte is 0x%x\n", *hva);
}

static void migration_mmio_write(void *opaque, hwaddr addr,
                                 uint64_t val, unsigned size)
{
    PCIDevice *dev = opaque;

    switch (addr) {
        case MI_STATE_CTL:
            handle_state_ctl_write(dev, val);
            break;
        case MI_LOG_CTL:
            printf("writing to log ctl is not implemented\n");
            break;

        case MI_STATE_BADDR_LO:
        case MI_STATE_BADDR_HI:
            read_addr_contents(dev);
            printf("val at 0x%lx is 0x%lx\n", addr, val);

            /* fall through */
        case MI_STATE_SIZE:
        case MI_LOG_SIZE:
        case MI_LOG_BADDR_LO:
        case MI_LOG_BADDR_HI:
            *(uint32_t *)(dev->migration_info + addr) = val;
            break;
        default:
            printf("Writing 0x%lx to mi device memory is not defined\n", addr);
    }

    return;
}

static const MemoryRegionOps migration_info_mmio_ops = {
    .read = migration_mmio_read,
    .write = migration_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

int migration_cap_init_bar(struct PCIDevice *dev, MemoryRegion *cfg_bar,
                           uint8_t cfg_bar_nr, unsigned cfg_offset,
                           uint8_t cap_pos, void *opaque, uint32_t dev_max_size,
                           Error **errp)
{

    int cap;
    uint8_t *config;
    unsigned migration_info_size = sizeof(struct migration_info);
    struct migration_info *mi;

    cap = pci_add_capability(dev, PCI_CAP_ID_MI, cap_pos, PCI_CAP_MI_SIZEOF, errp);

    if (cap < 0) {
        return cap;
    }

    dev->mi_opaque = opaque;
    dev->cap_present |= QEMU_PCI_CAP_MI;
    dev->migration_cap = cap;
    config = dev->config + cap;

    pci_set_long(config + PCI_MI_CONFIG, cfg_offset | cfg_bar_nr);

    dev->migration_info = g_malloc0(migration_info_size);
    mi = (struct migration_info *)dev->migration_info;
    mi->state_size = dev_max_size;

    memory_region_init_io(&dev->migration_info_mmio, OBJECT(dev),
                          &migration_info_mmio_ops, dev,
                          "migration-info", migration_info_size);
    memory_region_add_subregion(cfg_bar, cfg_offset, &dev->migration_info_mmio);

    return 0;
}

int migration_cap_init_exclusive_bar(PCIDevice *dev, uint8_t bar_nr, void *opaque,
                                     uint32_t dev_max_size, Error **errp)
{
    int ret;
    char *name;
    uint32_t bar_size = sizeof(struct migration_info);

    name = g_strdup_printf("%s-migration", dev->name);
    memory_region_init(&dev->migration_info_exclusive_bar, OBJECT(dev), name, bar_size);
    g_free(name);

    ret = migration_cap_init_bar(dev, &dev->migration_info_exclusive_bar, bar_nr,
                                 0, 0, opaque, dev_max_size, errp);

    if (ret) {
        return ret;
    }

    /* TODO: need PCI_BASE_ADDRESS_MEM_TYPE_64? */
    pci_register_bar(dev, bar_nr, PCI_BASE_ADDRESS_SPACE_IO,
                     &dev->migration_info_exclusive_bar);

    return 0;
}
