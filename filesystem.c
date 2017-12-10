 /*
    VRS Filesystem is currently a log-only filesystem.
    It only logs the function calls to the FUSE API, hence to the Linux filesystem.
 */

 /*
    Working Notes:

    1. Modify fuse_operations
    2. Replace log functions
 */

#define FUSE_USE_VERSION 31

//#include "config.h"
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

struct fuse_operations vrs_oper = {

                            /* create function...pending */
    .unlink = vrs_unlink,    /* delete function */
    .statfs = vrs_statfs,
    .open = vrs_open,
    .close = vrs_close,
    .read = vrs_read,
    .write = vrs_write,
    .readdir = vrs_readdir
    /*
    .getattr = bb_getattr,
    .readlink = bb_readlink,
    // no .getdir -- that's deprecated
    .getdir = NULL,
    .mknod = bb_mknod,
    .mkdir = bb_mkdir,
    .rmdir = bb_rmdir,
    .symlink = bb_symlink,
    .rename = bb_rename,
    .link = bb_link,
    .chmod = bb_chmod,
    .chown = bb_chown,
    .truncate = bb_truncate,
    .utime = bb_utime,
    .flush = bb_flush,
    .release = bb_release,
    .fsync = bb_fsync,


    .opendir = bb_opendir,

    .releasedir = bb_releasedir,
    .fsyncdir = bb_fsyncdir,
    .init = bb_init,
    .destroy = bb_destroy,
    .access = bb_access,
    .ftruncate = bb_ftruncate,
    .fgetattr = bb_fgetattr
    */
};

void vrs_fullpath(char fpath[PATH_MAX], const char *path){
    strcpy(fpath, VRS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here

    log_msg("vrs_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n", VRS_DATA->rootdir, path, fpath);
}

int vrs_unlink(const char *path){
    char fpath[PATH_MAX];

    log_msg("vrs_unlink(path=\"%s\")\n", path);
    vrs_fullpath(fpath, path);

    return log_syscall("unlink", unlink(fpath), 0);
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

int vrs_open(const char *path, struct fuse_file_info *fi){
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];

    log_msg("\nvrs_open(path\"%s\", fi=0x%08x)\n", path, fi);
    vrs_fullpath(fpath, path);

    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.
    fd = log_syscall("open", open(fpath, fi->flags), 0);
    if (fd < 0)
	   retstat = log_error("open");

    fi->fh = fd;
    log_fi(fi);

    return retstat;
}

int vrs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nvrs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
}

int vrs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;

    log_msg("\nvrs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
}

int vrs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    int retstat = 0;
    DIR *dp;
    struct dirent *de;

    log_msg("\nvrs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n", path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    log_msg("    readdir returned 0x%p\n", de);
    if(de == 0){
        retstat = log_error("vrs_readdir readdir");
        return retstat;
    }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
        log_msg("calling filler with name %s\n", de->d_name);
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            log_msg("    ERROR vrs_readdir filler:  buffer full");
            return -ENOMEM;
        }
    } while ((de = readdir(dp)) != NULL);
    log_fi(fi);

    return retstat;
}

int main(int argc, char* argv[]){

    int status;

    status = fuse_main(argc, argv, &vrs_oper, vrs_data);
    printf(stderr, "fuse_main returned %d\n", fuse_stat);

    return 0;
}
