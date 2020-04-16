/*
 * Copyright (c) 2019 MS-RTOS Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: ms_littlefs_cfg.h littlefs configuration.
 *
 * Author: Jiao.jinxing <jiaojixing@acoinfo.com>
 *
 */

#ifndef MS_LITTLEFS_CFG_H
#define MS_LITTLEFS_CFG_H

#include "ms_io_cfg.h"

/*
 * Maximum name size in bytes, may be redefined to reduce the size of the
 * info struct. Limited to <= 1022. Stored in superblock and must be
 * respected by other littlefs drivers.
 */
#define LFS_NAME_MAX    MS_CFG_IO_MAX_NAME

/*
 * Maximum size of a file in bytes, may be redefined to limit to support other
 * drivers. Limited on disk to <= 4294967296. However, above 2147483647 the
 * functions lfs_file_seek, lfs_file_size, and lfs_file_tell will return
 * incorrect values due to using signed integers. Stored in superblock and
 * must be respected by other littlefs drivers.
 */
#define LFS_FILE_MAX    (64U * 1024U * 1024U)

/*
 * Maximum size of custom attributes in bytes, may be redefined, but there is
 * no real benefit to using a smaller LFS_ATTR_MAX. Limited to <= 1022.
 */
#define LFS_ATTR_MAX    0U

#undef  LFS_YES_TRACE

#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_WARN
#define LFS_NO_ERROR

#undef  LFS_NO_INTRINSICS

#undef  LFS_NO_MALLOC

#endif /* MS_LITTLEFS_CFG_H */
