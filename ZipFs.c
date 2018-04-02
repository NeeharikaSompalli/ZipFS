#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <zip.h>
#include <fuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

static zip_t* archive;
static char* zname;

// Opening file function implementation
static int z_open(const char *path, struct fuse_file_info *fi)
{  (void) fi;
    printf("Opening file at %s\n", path);
    if(zip_name_locate(archive, path + 1, 0) < 0)
        return -ENOENT;     // Return error if the file is not being located by an index
    return 0;
}

//Function to close any opened file
static void z_close(void* data)
{
    (void) data;
    zip_close(archive);
}

//Getting the attributes of the path
static int z_getattr(const char *path, struct stat *st)
{   int len =strlen(path+1);
    printf("Getting attributes for the attributes requested in %s\n", path);
    if (strcmp(path, "/") == 0)             //If the path given is the base directory
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_size = 1;
        return 0;
    }
    zip_stat_t sb;
    zip_stat_init(&sb);
    char* addedslash = malloc(len + 2);
    strcpy(addedslash, (path+1));
    addedslash[len] ='/';
    addedslash[len + 1] = 0;
    char* slash = addedslash;
    if (strcmp(path, "/") != 0)
    {
    if (((int)zip_name_locate(archive, slash, 0)) != -1) // If the attribute is a directory
    {   zip_stat(archive, slash, 0, &sb);
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_size = 0;
        st->st_mtime = sb.mtime;
        free(slash);
        return 0;
    }
    else if (((int)zip_name_locate(archive, path + 1, 0)) != -1)// If the attribute is a regular file
    {
        zip_stat(archive, path + 1, 0, &sb);
        st->st_mode = S_IFREG | 0777;
        st->st_nlink = 1;
        st->st_size = sb.size;
        st->st_mtime = sb.mtime;
        free(slash);
        return 0;
    }
    else           //If its not afile or a directory
    {
        free(slash);
        return -ENOENT;
    }
    }
    return 0;
}

//reading the directory implementation
static int z_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info* fi)
{
    printf("Reading the directory at path  %s with  offset %lu\n", path, offset);
    
    (void) offset;
    (void) fi;
    filler(buf, ".", NULL, 0);       //current directory
    filler(buf, "..", NULL, 0);     // parent directory
    for (int i = 0; i < zip_get_num_entries(archive, 0); i++)
    {
        zip_stat_t sb;
        struct stat st;
        memset(&st, 0, sizeof(st));
        zip_stat_index(archive, i, 0, &sb);
        char* zippath = malloc(strlen(sb.name) + 2);
        *zippath = '/';
        strcpy(zippath + 1, sb.name);
        char* dirpath = strdup(zippath);    // duplicate the zip path to know if its a directory or not and also save basepath
        char* basepath = strdup(zippath);
        if (strcmp(path, dirname(dirpath)) == 0)
        {
            if (zippath[strlen(zippath) - 1] == '/')
            {zippath[strlen(zippath) - 1] = 0;
            }
            z_getattr(zippath, &st);
            char* name = basename(basepath);
            if (filler(buf, name, &st, 0))
                break;
        }
        free(dirpath);
        free(basepath);
        free(zippath);
    }
    return 0;
}

// reading the contents of the path
static int z_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info* fi)
{   int val;
    (void) fi;
    zip_stat_t sb;
    zip_stat_init(&sb);
    printf("Reading contents at %s with offset %lu\n", path, offset);
    zip_stat(archive, path + 1, 0, &sb);
    char filecontent[sb.size + size + offset]; // file content storing
    memset(filecontent, 0, sb.size + size + offset);
    zip_file_t* file = zip_fopen(archive, path + 1, 0);
    val = zip_fread(file, filecontent, sb.size);
    if (val == -1)
        return -ENOENT;   //if the zip file is not able to be read return error
    memcpy(buf, filecontent + offset, size);
    zip_fclose(file);     // Once the file reading is complete close the file
    return size;
}

// Function to check if the file is accessible or not
static int z_access(const char* path, int mask)
{
    int file_t=-1;
    int len =strlen(path+1);
    (void) mask;
    printf("Access availability for %s\n", path);
    if (strcmp(path, "/") == 0)
        file_t= 0;
    char* addedslash = malloc(len + 2);
    strcpy(addedslash, (path+1));
    addedslash[len] ='/';
    addedslash[len + 1] = 0;
    char* slash = addedslash;
    if (((int)zip_name_locate(archive, slash, 0)) != -1)
        file_t = 0;
    else if (((int)zip_name_locate(archive, path + 1, 0)) != -1)
        file_t = 1;
    else
        file_t = -1;
    free(slash);
    if (file_t >= 0)
        return 0;
    return -ENOENT;
}
static int z_statfs(const char* path, struct statvfs* stbuf){
    
    int res;
    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;
    return 0;
}
// As this is read only file system, all the write or content changing operations are returned with error
static int z_unlink(const char* path)
{
    (void)path;
    return -EROFS;
}
static int z_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    (void)path;
    (void)buf;
    (void)size;
    (void)offset;
    (void)fi;
    return -EROFS;
}
static int z_rmdir(const char *path)
{
    (void)path;
    return -EROFS;
}

static int z_mkdir(const char *path, mode_t mode)
{
    
    (void)path;
    (void) mode;
    return -EROFS;
}
static int z_rename(const char *from, const char *to)
{
    
    (void) from;
    (void) to;
    return -EROFS;
}
static int z_mknod(const char* path, mode_t mode, dev_t rdev)
{
    (void) path;
    (void) mode;
    (void) rdev;
    return -EROFS;
}
//Passing all the functions to the fuse functions 
static struct fuse_operations z_operations =
{   .getattr        = z_getattr,
    .readdir        = z_readdir,
    .access         = z_access,
    .open           = z_open,
    .read           = z_read,
    .destroy        = z_close,
    .mkdir          = z_mkdir,
    .mknod          = z_mknod,
    .unlink         = z_unlink,
    .rmdir          = z_rmdir,
    .write          = z_write,
    .statfs         = z_statfs,
    .rename         = z_rename,
};

int main(int argc, char *argv[])
{
    zname = argv[1];
    archive = zip_open(zname, 0, NULL); //open the archive name given as 1st argument
    if (!archive)
    {
        printf("Error while opening file\n");
        return -1;
    }
    char* argmts[argc - 1];
    argmts[0] = argv[0];
    printf("Reading the arguments given");
    for (int k = 1; k < argc - 1;k++)
    {
        argmts[k] = argv[k + 1];
    }
    printf("Passing all the arguments to fuse");
    return fuse_main(argc - 1, argmts, &z_operations, NULL);
}
/*References:
 1. https://pike.lysator.liu.se/generated/manual/modref/ex/predef_3A_3A/Fuse/Operations/statfs.html
 2. https://engineering.facile.it/blog/eng/write-filesystem-fuse/
 3. https://linux.die.net/man/3/
 4. https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201109/homework/fuse/fuse_doc.html
 5. https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
 6. http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
 7. https://gist.github.com/mobius/1759816, https://github.com/Ninja3047
 8. https://stackoverflow.com/questions/
 9.https://github.com/libfuse/libfuse/blob/master/example/hello.c
 10.https://libzip.org/documentation
 */


