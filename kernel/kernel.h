#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;     /* KiB below 1 MiB */
    uint32_t mem_upper;     /* KiB above 1 MiB */
} multiboot_info_t;

const multiboot_info_t *kernel_multiboot_info(void);

#endif
