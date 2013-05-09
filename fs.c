/*
 * rffs.c
 *
 *  Created on: May 8, 2013
 *      Author: Jinglei Ren <jinglei.ren@gmail.com>
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <linux/parser.h>
#include <linux/string.h>
#include <linux/magic.h>

#include "rffs.h"
#include "log.h"
#include "hashtable.h"

#define RFFS_DEFAULT_MODE 0755

struct rffs_mount_opts {
    umode_t mode;
};

struct rffs_fs_info {
    struct rffs_mount_opts mount_opts;
};

enum {
    Opt_mode, Opt_err
};

static const match_table_t tokens = {
    { Opt_mode, "mode=%o" }, { Opt_err, NULL }
};

static const struct super_operations rffs_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
    .show_options = generic_show_options,
};

static struct inode *base_inode;
DECLARE_HASHTABLE(inode_map, 8);
struct kmem_cache *rffs_inodep_cachep;

static int rffs_parse_options(char *data, struct rffs_mount_opts *opts) {
    substring_t args[MAX_OPT_ARGS];
    int option;
    int token;
    char *p;

    opts->mode = RFFS_DEFAULT_MODE;

    while ((p = strsep(&data, ",")) != NULL ) {
        if (!*p)
            continue;

        token = match_token(p, tokens, args);
        switch (token) {
        case Opt_mode:
            if (match_octal(&args[0], &option))
                return -EINVAL;
            opts->mode = option & S_IALLUGO;
            break;
            /*
             * We might like to report bad mount options here;
             * but traditionally rffs has ignored all mount options,
             * and as it is used as a !CONFIG_SHMEM simple substitute
             * for tmpfs, better continue to ignore other mount options.
             */
        }
    }

    return 0;
}

int rffs_fill_super(struct super_block *sb, void *data, int silent) {
    struct rffs_fs_info *fsi;
    struct inode *inode = NULL;
    struct dentry *root;
    struct inodep *inodep;
    int err;

    save_mount_options(sb, data);

    fsi = kzalloc(sizeof(struct rffs_fs_info), GFP_KERNEL);
    sb->s_fs_info = fsi;
    if (!fsi) {
        err = -ENOMEM;
        goto fail;
    }

    err = rffs_parse_options(data, &fsi->mount_opts);
    if (err)
        goto fail;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = RAMFS_MAGIC;
    sb->s_op = &rffs_ops;
    sb->s_time_gran = 1;

    inode = rffs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
    if (!inode) {
        err = -ENOMEM;
        goto fail;
    }

    root = d_alloc_root(inode);
    sb->s_root = root;
    if (!root) {
        err = -ENOMEM;
        goto fail;
    }

    // Register mapping to base inode
    inodep = inodep_malloc();
    inodep->ptr = base_inode;
    hash_add_inodep(inode_map, inode, inodep);
#ifdef RFFS_DEBUG
    PRINT("[RFFS] fill_super adds inode mapping: %lu - %lu\n",
            inode->i_ino, inodep->ptr->i_ino);
#endif
    return 0;

fail: kfree(fsi);
    sb->s_fs_info = NULL;
    iput(inode);
    return err;
}

struct dentry *rffs_mount(struct file_system_type *fs_type, int flags,
        const char *dev_name, void *data) {
    base_inode = current->fs->root.dentry->d_inode;
    return mount_nodev(fs_type, flags, data, rffs_fill_super);
}

static void rffs_kill_sb(struct super_block *sb) {
    kfree(sb->s_fs_info);
    kill_litter_super(sb);
}

static struct file_system_type rffs_fs_type = {
        .name = "rffs",
        .mount = rffs_mount,
        .kill_sb = rffs_kill_sb,
};

struct rffs_log rffs_log;

static int __init init_rffs_fs(void) {
    int err;

    log_init(&rffs_log);
    hash_init(inode_map);

    rffs_inodep_cachep = kmem_cache_create(
            "rffs_inodep_cache", sizeof(struct inodep), 0,
            (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
    if (!rffs_inodep_cachep) return -ENOMEM;

    err = register_filesystem(&rffs_fs_type);
    if (!err) PRINT("[RFFS] registered successfully.\n");
    return err;
}

static void __exit exit_rffs_fs(void) {
    kmem_cache_destroy(rffs_inodep_cachep);
    unregister_filesystem(&rffs_fs_type);
}

module_init(init_rffs_fs);
module_exit(exit_rffs_fs);

MODULE_LICENSE("GPL");
