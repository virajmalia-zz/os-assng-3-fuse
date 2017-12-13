/*
  VRS filesystem

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
*/

#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "log.h"

// Get Full path from rootDir
static void vrs_fullpath(char fpath[PATH_MAX], const char *path){
    strcpy(fpath, VRS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    log_msg("vrs_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n", VRS_DATA->rootdir, path, fpath);
}

void *vrs_init(struct fuse_conn_info *conn){
    log_msg("\nvrs_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());

    return VRS_DATA;
}

void vrs_destroy(void *userdata){
    log_msg("\nvrs_destroy(userdata=0x%08x)\n", userdata);
    disk_close();

    free(VRS_DATA->state_inodes);
    VRS_DATA->state_inodes = NULL;

    free(VRS_DATA->state_data_blocks);
    VRS_DATA->state_data_blocks = NULL;

    VRS_DATA->free_inodes = NULL;
    VRS_DATA->free_data_blocks = NULL;
}

int vrs_getattr(const char *path, struct stat *statbuf){
    int retstat = 0;
    char fpath[PATH_MAX];

    log_msg("\nvrs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);

    uint32_t ino = path_2_ino(path);
    if (ino != VRS_INVALID_INO) {
        log_msg("\nvrs_getattr path found");
        vrs_inode_t inode;
        get_inode(ino, &inode);
        fill_stat_from_ino(&inode, statbuf);
    }
    else {
        log_msg("\nvrs_getattr path not found");
        retstat = -ENOENT;
    }

    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int vrs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nvrs_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);
    log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
        return vrs_getattr(path, statbuf);

    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	   retstat = log_error("vrs_fgetattr fstat");

    log_stat(statbuf);

    return retstat;
}

int vrs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);
    uint32_t ino = create_inode(path, mode);
    log_msg("\nFile creation success inode = %d", ino);

    return retstat;
}

/** Remove a file */
int vrs_unlink(const char *path){
    int retstat = 0;
    log_msg("vrs_unlink(path=\"%s\")\n", path);
    retstat = remove_inode(path);

    return retstat;
}

int vrs_open(const char *path, struct fuse_file_info *fi){
    int retstat = -ENOENT;
    log_msg("\nvrs_open(path\"%s\", fi=0x%08x)\n", path, fi);

	uint32_t ino = path_2_ino(path);
	if (ino != VRS_INVALID_INO) {
		vrs_inode_t inode;
		get_inode(ino, &inode);
		if (S_ISREG(inode.mode)) {
			retstat = 0;
		}
	}
    else {
		log_msg("\nNot a valid file");
	}

    return retstat;
}

int vrs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;
    log_msg("\nvrs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

	uint32_t ino = path_2_ino(path);
	if (ino != VRS_INVALID_INO) {
		log_msg("\nvrs_read path found");
		vrs_inode_t inode;
		get_inode(ino, &inode);
		log_msg("\nvrs_read got the inode");
		retstat = read_inode(&inode, buf, size, offset);
		log_msg("\nData read = %s", buf);
	}
    else {
		log_msg("\nvrs_read path not found");
		retstat = -ENOENT;
	}

    return retstat;
}

int vrs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;
    log_msg("\nvrs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

	uint32_t ino = path_2_ino(path);
	if (ino != VRS_INVALID_INO) {
		log_msg("\nvrs_write path found");
		vrs_inode_t inode;
		get_inode(ino, &inode);
		retstat = write_inode(&inode, buf, size, offset);
	}
    else {
		log_msg("\nvrs_write path not found");
		retstat = -ENOENT;
	}

    return retstat;
}

int vrs_release(const char *path, struct fuse_file_info *fi){
    int retstat = 0;
    log_msg("\nvrs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

    // No saved data related to any file which needs to be freed.

    return retstat;
}

int vrs_statfs(const char *path, struct statvfs *statv){
    int retstat = 0;
    char fpath[PATH_MAX];

    log_msg("\nvrs_statfs(path=\"%s\", statv=0x%08x)\n", path, statv);
    vrs_fullpath(fpath, path);

    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    log_statvfs(statv);

    return retstat;
}


/** Create a directory */
int vrs_mkdir(const char *path, mode_t mode){
    int retstat = 0;
    log_msg("\nvrs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

    uint32_t ino = create_inode(path, mode);
    log_msg("\nFile creation success inode = %d", ino);

    return retstat;
}

/** Remove a directory */
int vrs_rmdir(const char *path){
    int retstat = 0;
    log_msg("vrs_rmdir(path=\"%s\")\n", path);

    return retstat;
}

int vrs_opendir(const char *path, struct fuse_file_info *fi){
    int retstat = 0;
    log_msg("\nvrs_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);

	uint32_t ino = path_2_ino(path);
	if (ino != VRS_INVALID_INO) {
        vrs_inode_t inode;
        get_inode(ino, &inode);
        if (S_ISDIR(inode.mode)) {
			retstat = 0;
        }
    }
    else {
        log_msg("\nNot a valid file");
    }

    return retstat;
}

int vrs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nvrs_readdir(path=\"%s\")\n", path);

    filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	uint32_t ino = path_2_ino(path);
	if (ino != VRS_INVALID_INO) {
		log_msg("\nvrs_readdir path found");
		vrs_inode_t inode;
		get_inode(ino, &inode);

		int num_dentries = (inode.size / VRS_DENTRY_SIZE);
		vrs_dentry_t* dentries = malloc(sizeof(vrs_dentry_t) * num_dentries);
	    read_dentries(&inode, dentries);

	    int i = 0;
	    for (i = 0; i < num_dentries; ++i) {
	    	filler(buf, dentries[i].name, NULL, 0);
	    }

	    free(dentries);
	}
    else {
		log_msg("\nvrs_readdir path not found");
	}

    return retstat;
}

int vrs_releasedir(const char *path, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nvrs_releasedir(path=\"%s\", fi=0x%08x)\n", path, fi);
    log_fi(fi);
    closedir((DIR *) (uintptr_t) fi->fh);

    return retstat;
}

struct fuse_operations vrs_oper = {
    .init = vrs_init,
    .destroy = vrs_destroy,
    .getattr = vrs_getattr,
    .fgetattr = vrs_fgetattr,

    .create = vrs_create,
    .unlink = vrs_unlink,

    .open = vrs_open,
    .release = vrs_release,
    .read = vrs_read,
    .write = vrs_write,
    .statfs = vrs_statfs,

    .mkdir = vrs_mkdir,
    .rmdir = vrs_rmdir,

    .opendir = vrs_opendir,
    .readdir = vrs_readdir,
    .releasedir = vrs_releasedir
};

void vrs_usage(){
    fprintf(stderr, "usage:  vrvrs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[]){
    int fuse_stat;
    struct vrs_state *vrs_data;

    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	vrs_usage();

    vrs_data = malloc(sizeof(struct vrs_state));
    if (vrs_data == NULL) {
	       perror("main calloc");
	       abort();
    }

    // Pull the rootdir out of the argument list and save it in my internal data
    vrs_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    vrs_data->logfile = log_open();

    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &vrs_oper, vrs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    return fuse_stat;
}
