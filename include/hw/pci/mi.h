#ifndef QEMU_MI_H
#define QEMU_MI_H

#include "qemu-common.h"
#include "hw/pci/pci.h"


struct MigrationOps{
    void (*set_addr)(void *opaque, void *iov, int size);
    void (*start)(void *opaque);
    void (*stop)(void *opaque);
};

void register_migration_ops(PCIDevice *dev, const struct MigrationOps *ops,
                            void *opaque);
void migration_write_config(PCIDevice *dev, uint32_t addr,
                            uint32_t val, int len);

void migration_cap_init(PCIDevice *dev, Error **errp);

int migration_cap_init_bar(struct PCIDevice *dev, MemoryRegion *cfg_bar,
                           uint8_t cfg_bar_nr, unsigned cfg_offset,
                           uint8_t cap_pos, void *opaque, uint32_t dev_max_size,
                           Error **errp);

int migration_cap_init_exclusive_bar(PCIDevice *dev, uint8_t bar_nr, void *opaque,
                                     uint32_t dev_max_size, Error **errp);

struct migration_info {
    /* read: not available
     * write: 0 - reset the state registers
     *        1 - save the state
     *        2 - restore the state
     */
    uint32_t state_ctl;
    /* read: the max device state size on reset
     *       the real device state size after save the state
     * write: the size of the baddr
     */
    uint32_t state_size;
    uint32_t state_baddr_lo;
    uint32_t state_baddr_hi;

    /* read: not available
     * write: 0 - reset the log registers
     *        1 - log enable
     *        2 - log disable
     */
    uint32_t log_ctl;
    /* read: not available
     * write: the size of the log
     */
    uint32_t log_size;
    uint32_t log_baddr_lo;
    uint32_t log_baddr_hi;
};

#define MI_STATE_CTL        0
#define   MI_STATE_CTL_RESET 0
#define   MI_STATE_CTL_SAVE  1
#define   MI_STATE_CTL_RESTORE 2
#define MI_STATE_SIZE       4
#define MI_STATE_BADDR_LO   8
#define MI_STATE_BADDR_HI   12
#define MI_LOG_CTL          16
#define   MI_LOG_CTL_RESET   0
#define   MI_LOG_CTL_ENABLE  1
#define   MI_LOG_CTL_DISABLE 2
#define MI_LOG_SIZE         20
#define MI_LOG_BADDR_LO     24
#define MI_LOG_BADDR_HI     28

#endif
