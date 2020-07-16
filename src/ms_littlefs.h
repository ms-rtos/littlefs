/*
 * Copyright (c) 2015-2020 ACOINFO Co., Ltd.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: ms_littlefs.h littlefs implement.
 *
 * Author: Jiao.jinxing <jiaojinxing@acoinfo.com>
 *
 */

#ifndef MS_LITTLEFS_H
#define MS_LITTLEFS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * for struct lfs_config
 */
#include "littlefs/lfs.h"

#define MS_LITTLEFS_NAME    "littlefs"

ms_err_t ms_littlefs_register(void);

#ifdef __cplusplus
}
#endif

#endif /* MS_LITTLEFS_H */
