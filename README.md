
## littlefs

A little fail-safe filesystem designed for microcontrollers.

License: BSD-3-Clause license.

Download: `https://github.com/ARMmbed/littlefs/releases`

Version: 2.2.1

Example:

```c
#define __MS_IO
#include "ms_kern.h"
#include "ms_io_core.h"
#include "fs/ms_littlefs.h"

/* configuration of the filesystem is provided by this struct */
static const struct lfs_config xxx_cfg = {
    // block device operations
    .read  = user_provided_block_device_read,
    .prog  = user_provided_block_device_prog,
    .erase = user_provided_block_device_erase,
    .sync  = user_provided_block_device_sync,

    // block device configuration
    .read_size = 16,
    .prog_size = 16,
    .block_size = 4096,
    .block_count = 128,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

static ms_io_device_t xxx_dev;
static ms_io_mnt_t xxx_mnt;

ms_io_device_register(&xxx_dev, "/dev/xxx_blk", "xxx_drv", &xxx_cfg);

ms_littlefs_register();

ms_io_mount(&xxx_mnt, "/xxx_dir", "/dev/xxx_blk", MS_LITTLEFS_NAME, MS_NULL);

bsp link liblittlefs.a, LOCAL_DEPEND_LIB add -llittlefs:

LOCAL_DEPEND_LIB      := -llittlefs

```
