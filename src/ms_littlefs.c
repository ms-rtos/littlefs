/*
 * Copyright (c) 2019 MS-RTOS Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: ms_littlefs.c littlefs implement.
 *
 * Author: Jiao.jinxing <jiaojixing@acoinfo.com>
 *
 */

#define __MS_IO
#include "ms_kern.h"
#include "ms_io_core.h"
#include "ms_littlefs.h"

#include <string.h>
#include <stdio.h>

/**
 * @brief littlefs file system.
 */

#define LFS_TYPE_MASK   0x03U

typedef struct {
    lfs_t           lfs;
    ms_handle_t     lock;
} ms_lfs_t;

static int __ms_littlefs_err_to_errno(int err)
{
    switch (err) {
    case LFS_ERR_OK:
        err = 0;
        break;

    case LFS_ERR_IO:
        err = EIO;
        break;

    case LFS_ERR_CORRUPT:
        err = EFAULT;
        break;

    case LFS_ERR_NOENT:
        err = ENOENT;
        break;

    case LFS_ERR_EXIST:
        err = EEXIST;
        break;

    case LFS_ERR_NOTDIR:
        err = ENOTDIR;
        break;

    case LFS_ERR_ISDIR:
        err = EISDIR;
        break;

    case LFS_ERR_NOTEMPTY:
        err = ENOTEMPTY;
        break;

    case LFS_ERR_BADF:
        err = EBADF;
        break;

    case LFS_ERR_FBIG:
        err = EFBIG;
        break;

    case LFS_ERR_INVAL:
        err = EINVAL;
        break;

    case LFS_ERR_NOSPC:
        err = ENOSPC;
        break;

    case LFS_ERR_NOMEM:
        err = ENOMEM;
        break;

    case LFS_ERR_NOATTR:
        err = EINVAL;
        break;

    case LFS_ERR_NAMETOOLONG:
        err = ENAMETOOLONG;
        break;

    default:
        err = EFAULT;
        break;
    }

    return err;
}

static int __ms_oflag_to_littlefs_oflag(int oflag)
{
    int ret = 0;

    switch (oflag & O_ACCMODE) {
    case O_RDONLY:
        ret |= LFS_O_RDONLY;
        break;

    case O_WRONLY:
        ret |= LFS_O_WRONLY;
        break;

    case O_RDWR:
        ret |= LFS_O_RDWR;
        break;
    }

    if (oflag & O_APPEND) {
        ret |= LFS_O_APPEND;
    }

    if (oflag & O_TRUNC) {
        ret |= LFS_O_TRUNC;
    }

    if (oflag & O_EXCL) {
        ret |= LFS_O_EXCL;
    }

    if (oflag & O_CREAT) {
        ret |= LFS_O_CREAT;
    }

    return ret;
}

static int __ms_whence_to_littlefs_whence(int whence)
{
    switch (whence) {
    case SEEK_SET:
        whence = LFS_SEEK_SET;
        break;

    case SEEK_CUR:
        whence = LFS_SEEK_CUR;
        break;

    case SEEK_END:
        whence = LFS_SEEK_END;
        break;

    default:
        whence = -1;
        break;
    }

    return whence;
}

static ms_mode_t __ms_littlefs_file_type_to_mode(ms_uint8_t type)
{
    ms_mode_t ret;

    switch (type & LFS_TYPE_MASK) {
    case LFS_TYPE_REG:
        ret = S_IFREG;
        break;

    case LFS_TYPE_DIR:
        ret = S_IFDIR;
        break;

    default:
        ret = 0;
        break;
    }

    return ret;
}

static ms_uint8_t __ms_littlefs_file_type_to_type(ms_uint8_t type)
{
    ms_uint8_t ret;

    switch (type & LFS_TYPE_MASK) {
    case LFS_TYPE_REG:
        ret = DT_REG;
        break;

    case LFS_TYPE_DIR:
        ret = DT_DIR;
        break;

    default:
        ret = DT_UNKNOWN;
        break;
    }

    return ret;
}

static void __ms_little_fs_lock(ms_lfs_t *lfs)
{
    while (ms_mutex_lock(lfs->lock, MS_TIMEOUT_FOREVER) != MS_ERR_NONE) {
    }
}

static void __ms_little_fs_unlock(ms_lfs_t *lfs)
{
    (void)ms_mutex_unlock(lfs->lock);
}

static int __ms_littlefs_mount(ms_io_mnt_t *mnt, ms_io_device_t *dev, const char *dev_name, ms_const_ptr_t param)
{
    ms_lfs_t *lfs;
    int ret;

    if ((dev != MS_NULL) && (dev->ctx != MS_NULL)) {
        lfs = ms_kzalloc(sizeof(ms_lfs_t));
        if (lfs != MS_NULL) {

            if (ms_mutex_create("lfs_lock", MS_WAIT_TYPE_PRIO, &lfs->lock) == MS_ERR_NONE) {

                ret = lfs_mount(&lfs->lfs, dev->ctx);
                if (ret < 0) {

                    ret = lfs_format(&lfs->lfs, dev->ctx);
                    if (ret == 0) {
                        ret = lfs_mount(&lfs->lfs, dev->ctx);
                    }
                }

                if (ret < 0) {
                    (void)ms_mutex_destroy(lfs->lock);
                    ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
                    ret = -1;

                } else {
                    mnt->ctx = lfs;
                    ret = 0;
                }

            } else {
                ms_thread_set_errno(ENOMEM);
                ret = -1;
            }

            if (ret < 0) {
                (void)ms_kfree(lfs);
            }

        } else {
            ms_thread_set_errno(ENOMEM);
            ret = -1;
        }

    } else {
        ms_thread_set_errno(EFAULT);
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_mkfs(ms_io_mnt_t *mnt, ms_const_ptr_t param)
{
    ms_lfs_t *lfs = mnt->ctx;
    int ret;

    __ms_little_fs_lock(lfs);

    ret = lfs_unmount(&lfs->lfs);
    if (ret == 0) {
        ret = lfs_format(&lfs->lfs, mnt->dev->ctx);
        if (ret == 0) {
            ret = lfs_mount(&lfs->lfs, mnt->dev->ctx);
        }
    }

    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_unmount(ms_io_mnt_t *mnt, ms_const_ptr_t param, ms_bool_t force)
{
    ms_lfs_t *lfs = mnt->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_unmount(&lfs->lfs);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        mnt->ctx = MS_NULL;
        (void)ms_kfree(lfs);
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_open(ms_io_mnt_t *mnt, ms_io_file_t *file, const char *path, int oflag, ms_mode_t mode)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file;
    int ret;

    (void)mode;

    lfs_file = ms_kzalloc(sizeof(lfs_file_t));
    if (lfs_file != MS_NULL) {
        oflag = __ms_oflag_to_littlefs_oflag(oflag);

        __ms_little_fs_lock(lfs);
        ret = lfs_file_open(&lfs->lfs, lfs_file, path, oflag);
        __ms_little_fs_unlock(lfs);

        if (ret < 0) {
            (void)ms_kfree(lfs_file);
            ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
            ret = -1;
        } else {
            file->ctx = lfs_file;
            ret = 0;
        }

    } else {
        ms_thread_set_errno(ENOMEM);
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_close(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_file_close(&lfs->lfs, lfs_file);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        (void)ms_kfree(lfs_file);
        file->ctx = MS_NULL;
        ret = 0;
    }

    return ret;
}

static ms_ssize_t __ms_littlefs_read(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_ptr_t buf, ms_size_t len)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    ms_ssize_t ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_file_read(mnt->ctx, lfs_file, buf, len);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    }

    return ret;
}

static ms_ssize_t __ms_littlefs_write(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_const_ptr_t buf, ms_size_t len)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    ms_ssize_t ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_file_write(mnt->ctx, lfs_file, buf, len);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_fcntl(ms_io_mnt_t *mnt, ms_io_file_t *file, int cmd, int arg)
{
    int ret;

    switch (cmd) {
    case F_GETFL:
        ret = file->flags;
        break;

    case F_SETFL:
        if ((!(file->flags & FWRITE)) && (arg & FWRITE)) {
            ms_thread_set_errno(EACCES);
            ret = -1;
        } else {
            file->flags = arg;
            ret = 0;
        }
        break;

    default:
        ms_thread_set_errno(EINVAL);
        ret = -1;
        break;
    }

    return ret;
}

static int __ms_littlefs_fstat(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_stat_t *buf)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    int ret;

    bzero(buf, sizeof(ms_stat_t));

    __ms_little_fs_lock(lfs);
    ret = lfs_file_size(&lfs->lfs, lfs_file);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        buf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFREG;
        buf->st_size = ret;
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_isatty(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    return 0;
}

static int __ms_littlefs_fsync(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_file_sync(&lfs->lfs, lfs_file);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_ftruncate(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_off_t len)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_file_truncate(&lfs->lfs, lfs_file, len);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static ms_off_t __ms_littlefs_lseek(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_off_t offset, int whence)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file = file->ctx;
    ms_off_t ret;

    whence = __ms_whence_to_littlefs_whence(whence);

    __ms_little_fs_lock(lfs);
    ret = lfs_file_seek(&lfs->lfs, lfs_file, offset, whence);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_stat(ms_io_mnt_t *mnt, const char *path, ms_stat_t *buf)
{
    ms_lfs_t *lfs = mnt->ctx;
    struct lfs_info linfo;
    int ret;

    bzero(buf, sizeof(ms_stat_t));

    if (MS_IO_PATH_IS_ROOT(path)) {
        path = "/";
    }

    __ms_little_fs_lock(lfs);
    ret = lfs_stat(&lfs->lfs, path, &linfo);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        buf->st_mode  = S_IRWXU | S_IRWXG | S_IRWXO;
        buf->st_mode |= __ms_littlefs_file_type_to_mode(linfo.type);
        buf->st_size  = ((linfo.type & LFS_TYPE_MASK) == LFS_TYPE_REG) ? \
                        linfo.size : 0;
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_statvfs(ms_io_mnt_t *mnt, ms_statvfs_t *buf)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_ssize_t fs_size;
    int ret;

    __ms_little_fs_lock(lfs);
    fs_size = lfs_fs_size(&lfs->lfs);
    __ms_little_fs_unlock(lfs);

    if (fs_size < 0) {
        bzero(buf, sizeof(ms_statvfs_t));
        ms_thread_set_errno(__ms_littlefs_err_to_errno(fs_size));
        ret = -1;
    } else {
        struct lfs_config *cfg = mnt->dev->ctx;

        buf->f_bsize  = cfg->block_size;
        buf->f_frsize = cfg->prog_size;
        buf->f_blocks = cfg->block_count;
        buf->f_bfree  = buf->f_blocks - fs_size;
        buf->f_files  = 0UL;
        buf->f_ffree  = 0UL;
        buf->f_dev    = mnt->dev->nnode.name;
        buf->f_mnt    = mnt->nnode.name;
        buf->f_fsname = MS_LITTLEFS_NAME;
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_unlink(ms_io_mnt_t *mnt, const char *path)
{
    ms_lfs_t *lfs = mnt->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_remove(&lfs->lfs, path);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_mkdir(ms_io_mnt_t *mnt, const char *path, ms_mode_t mode)
{
    ms_lfs_t *lfs = mnt->ctx;
    int ret;

    (void)mode;

    __ms_little_fs_lock(lfs);
    ret = lfs_mkdir(&lfs->lfs, path);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_rename(ms_io_mnt_t *mnt, const char *old, const char *_new)
{
    ms_lfs_t *lfs = mnt->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_rename(&lfs->lfs, old, _new);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_sync(ms_io_mnt_t *mnt)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t *lfs_file;
    int ret = 0;

    __ms_little_fs_lock(lfs);

    for (lfs_file = (lfs_file_t *)lfs->lfs.mlist; lfs_file != MS_NULL; lfs_file = lfs_file->next) {
        if ((lfs_file->type & LFS_TYPE_MASK) != LFS_TYPE_REG) {
            continue;
        }

        ret = lfs_file_sync(&lfs->lfs, lfs_file);
        if (ret < 0) {
            break;
        }
    }

    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_truncate(ms_io_mnt_t *mnt, const char *path, ms_off_t len)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_file_t lfs_file;
    int oflag = __ms_oflag_to_littlefs_oflag(O_WRONLY);
    int ret;

    __ms_little_fs_lock(lfs);

    ret = lfs_file_open(&lfs->lfs, &lfs_file, path, oflag);
    if (ret >= 0) {
        ret = lfs_file_truncate(&lfs->lfs, &lfs_file, len);

        (void)lfs_file_close(&lfs->lfs, &lfs_file);
    }

    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_opendir(ms_io_mnt_t *mnt, ms_io_file_t *file, const char *path)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir;
    int ret;

    if (MS_IO_PATH_IS_ROOT(path)) {
        path = "/";
    }

    lfs_dir = ms_kzalloc(sizeof(lfs_dir_t));
    if (lfs_dir != MS_NULL) {

        __ms_little_fs_lock(lfs);
        ret = lfs_dir_open(&lfs->lfs, lfs_dir, path);
        __ms_little_fs_unlock(lfs);

        if (ret < 0) {
            (void)ms_kfree(lfs_dir);
            ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
            ret = -1;
        } else {
            file->ctx = lfs_dir;
            ret = 0;
        }

    } else {
        ms_thread_set_errno(ENOMEM);
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_closedir(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_dir_close(&lfs->lfs, lfs_dir);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        file->ctx = MS_NULL;
        (void)ms_kfree(lfs_dir);
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_readdir_r(ms_io_mnt_t *mnt, ms_io_file_t *file, ms_dirent_t *entry, ms_dirent_t **result)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir = file->ctx;
    struct lfs_info linfo;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_dir_read(&lfs->lfs, lfs_dir, &linfo);
    __ms_little_fs_unlock(lfs);

    if (ret > 0) {
        strlcpy(entry->d_name, linfo.name, sizeof(entry->d_name));
        entry->d_type = __ms_littlefs_file_type_to_type(linfo.type);

        if (result != MS_NULL) {
            *result = entry;
        }

        ret = 1;

    } else if (ret == 0) {
        if (result != MS_NULL) {
            *result = MS_NULL;
        }

    } else {
        if (result != MS_NULL) {
            *result = MS_NULL;
        }

        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    }

    return ret;
}

static int __ms_littlefs_rewinddir(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_dir_rewind(&lfs->lfs, lfs_dir);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static int __ms_littlefs_seekdir(ms_io_mnt_t *mnt, ms_io_file_t *file, long loc)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir = file->ctx;
    int ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_dir_seek(&lfs->lfs, lfs_dir, loc);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    } else {
        ret = 0;
    }

    return ret;
}

static long __ms_littlefs_telldir(ms_io_mnt_t *mnt, ms_io_file_t *file)
{
    ms_lfs_t *lfs = mnt->ctx;
    lfs_dir_t *lfs_dir = file->ctx;
    long ret;

    __ms_little_fs_lock(lfs);
    ret = lfs_dir_tell(&lfs->lfs, lfs_dir);
    __ms_little_fs_unlock(lfs);

    if (ret < 0) {
        ms_thread_set_errno(__ms_littlefs_err_to_errno(ret));
        ret = -1;
    }

    return ret;
}

static ms_io_fs_ops_t ms_io_littlefs_ops = {
        .type       = MS_IO_FS_TYPE_DISKFS,
        .mount      = __ms_littlefs_mount,
        .unmount    = __ms_littlefs_unmount,
        .mkfs       = __ms_littlefs_mkfs,

        .link       = MS_NULL,
        .unlink     = __ms_littlefs_unlink,
        .mkdir      = __ms_littlefs_mkdir,
        .rmdir      = __ms_littlefs_unlink,
        .rename     = __ms_littlefs_rename,
        .sync       = __ms_littlefs_sync,
        .truncate   = __ms_littlefs_truncate,
        .stat       = __ms_littlefs_stat,
        .lstat      = __ms_littlefs_stat,
        .statvfs    = __ms_littlefs_statvfs,

        .open       = __ms_littlefs_open,
        .close      = __ms_littlefs_close,
        .read       = __ms_littlefs_read,
        .write      = __ms_littlefs_write,
        .ioctl      = MS_NULL,
        .fcntl      = __ms_littlefs_fcntl,
        .fstat      = __ms_littlefs_fstat,
        .isatty     = __ms_littlefs_isatty,
        .fsync      = __ms_littlefs_fsync,
        .fdatasync  = __ms_littlefs_fsync,
        .ftruncate  = __ms_littlefs_ftruncate,
        .lseek      = __ms_littlefs_lseek,
        .poll       = MS_NULL,

        .opendir    = __ms_littlefs_opendir,
        .closedir   = __ms_littlefs_closedir,
        .readdir_r  = __ms_littlefs_readdir_r,
        .rewinddir  = __ms_littlefs_rewinddir,
        .seekdir    = __ms_littlefs_seekdir,
        .telldir    = __ms_littlefs_telldir,
};

static ms_io_fs_t ms_io_littlefs = {
        .nnode = {
            .name = MS_LITTLEFS_NAME,
        },
        .ops = &ms_io_littlefs_ops,
};

ms_err_t ms_littlefs_register(void)
{
    return ms_io_fs_register(&ms_io_littlefs);
}
