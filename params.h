// maintain vrsfs state in here
#include <limits.h>
#include <stdio.h>

struct vrs_state {
    FILE *logfile;
    char *rootdir;
};

#define VRS_DATA ((struct vrs_state *) fuse_get_context()->private_data)
