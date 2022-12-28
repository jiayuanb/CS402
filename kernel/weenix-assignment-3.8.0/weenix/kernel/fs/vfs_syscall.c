/******************************************************************************/
/* Important Fall 2022 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.2 2018/05/27 03:57:26 cvsps Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"



/*
 * Syscalls for vfs. Refer to comments or man pages for implementation.
 * Do note that you don't need to set errno, you should just return the
 * negative error code.
 */

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read vn_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        if(fd < 0){
                return -EBADF;
        }

        file_t* my_file = fget(fd);
        if (my_file == NULL){
                return -EBADF;
        }else if(!(my_file->f_mode & FMODE_READ)){
                fput(my_file);
                return -EBADF;
        }
        if(S_ISDIR(my_file -> f_vnode -> vn_mode)){
                fput(my_file);
                return -EISDIR;
        }
       
       
        int read_size = my_file->f_vnode->vn_ops->read(my_file->f_vnode, my_file->f_pos, buf, nbytes);

        my_file->f_pos += read_size;
        fput(my_file);
        
        
        return read_size;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * vn_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
        if(fd < 0){
                return -EBADF;
        }
        file_t* my_file = fget(fd);
        if(my_file == NULL ){
                return -EBADF;
        }else if(!(my_file->f_mode & FMODE_WRITE)){
                fput(my_file);
                return -EBADF;
        }
        int result = 0;
         if((my_file->f_mode & FMODE_WRITE) == FMODE_WRITE){
                if((my_file->f_mode & FMODE_APPEND) == FMODE_APPEND){
                        do_lseek(fd, 0, SEEK_END);
                        result = my_file->f_vnode->vn_ops->write(my_file->f_vnode, my_file->f_pos, buf, nbytes);
                        my_file->f_pos += result;
                }
                else{
                        result = my_file->f_vnode->vn_ops->write(my_file->f_vnode, my_file->f_pos, buf, nbytes);
                        my_file->f_pos += result;
                 }
        }

        fput(my_file);
        return result;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{ 
        if (fd < 0 || fd >= NFILES || curproc->p_files[fd] == NULL){
                return -EBADF;
        }

        fput(curproc->p_files[fd]);     
        curproc->p_files[fd] = NULL;
        return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        if (fd < 0 || fd >= NFILES || curproc -> p_files[fd]==NULL){
                return -EBADF;
        }


        file_t* old_file = fget(fd);
        int next_empty_id = get_empty_fd(curproc);
        if(next_empty_id < 0){
                // fput(old_file);
                return -EMFILE;
        }


        curproc->p_files[next_empty_id] = old_file;
        return next_empty_id;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        if (ofd < 0 || ofd >= NFILES || nfd < 0 || nfd >= NFILES || curproc->p_files[ofd] == NULL){
                return -EBADF;
        }
        /* NOT_YET_IMPLEMENTED("VFS: do_dup2"); */
        if(nfd != ofd && curproc -> p_files[nfd] != NULL){
                do_close(nfd);
        }else if(nfd == ofd){
                return nfd;
        }

        curproc -> p_files[nfd] = fget(ofd);
        return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{

        size_t namelen;
        const char * buf;
        vnode_t *base = NULL;//curent path inode
        vnode_t *parent_vnode = NULL;
        vnode_t *child_vnode = NULL;
        if(dir_namev(path, &namelen, &buf, base, &parent_vnode) > 0){
                vput(parent_vnode);
                if(lookup(parent_vnode, (const char *) buf, namelen, &child_vnode) < 0){
                        vfs_root_vn->vn_ops->mknod(parent_vnode, buf, namelen, mode, devid);
                }
                else{
                        vput(child_vnode);
                }
        }
        // NOT_YET_IMPLEMENTED("VFS: do_mknod");
        return 0;
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{

        size_t namelen;
        const char * buf;
        vnode_t *base = curproc->p_cwd;//curent path inode
        vnode_t *parent_vnode = NULL;
        int result = 0;
        if(strlen(path) > NAME_LEN){
                return -ENAMETOOLONG;
        }

        if((result = dir_namev(path, &namelen, &buf, base, &parent_vnode)) > 0){//no such dir
                vnode_t *child_vnode = NULL;
                if(namelen != 0){

                        result = lookup(parent_vnode, (const char *) buf, namelen, &child_vnode);
                        if(result < 0){
                                result = vfs_root_vn->vn_ops->mkdir(parent_vnode, buf, namelen);
                         }else{
                                 result = -EEXIST;
                                vput(child_vnode);
                        }
                }

                if((path[0] == '/') && (namelen == (strlen(path)-1))){
                         return result;
                }else if((parent_vnode != curproc->p_cwd)){
                        vput(parent_vnode);
                }
        }
 
        // NOT_YET_IMPLEMENTED("VFS: do_mkdir");
        return result;
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{

        size_t namelen;
        const char * buf;
        int result = 0;
        vnode_t *base = curproc->p_cwd;//curent path inode
        vnode_t *parent_vnode = NULL;
        if((result = dir_namev(path, &namelen, &buf, base, &parent_vnode)) >= 0){//no such dir
                if(!strcmp(".", buf)){
                        result = -EINVAL;
                }else if(!strcmp("..", buf)){
                        result = -ENOTEMPTY;
                }else{
                        vnode_t *child_vnode = NULL;
                        result = parent_vnode->vn_ops->rmdir(parent_vnode, (const char *) buf, namelen);

                }

                if((path[0] == '/') && (namelen == (strlen(path)-1))){

                        parent_vnode = parent_vnode;
                }else if(parent_vnode != curproc->p_cwd){
                        vput(parent_vnode);
                }
        }
        return result;
}

/*
 * Similar to do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EPERM
 *        path refers to a directory.
 *      o ENOENT
 *        Any component in path does not exist, including the element at the
 *        very end.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{

        size_t namelen;
        const char * buf;
        int result = -ENOTDIR;
        vnode_t *base = curproc->p_cwd;//curent path inode
        vnode_t *parent_vnode = NULL;
        if((result = dir_namev(path, &namelen, &buf, base, &parent_vnode)) > 0){//no such dir
                vnode_t *child_vnode = NULL;
                result = lookup(parent_vnode, (const char *) buf, namelen, &child_vnode);
                if(result >= 0){
                        if(!S_ISDIR(child_vnode->vn_mode)){
                                result = curproc->p_cwd->vn_ops->unlink(parent_vnode, (const char *) buf, namelen);
                        }else{
                                result = -EPERM;
                        }
                        vput(child_vnode);
                }
                else{
                        result = -ENOENT;
                }
                if(parent_vnode != curproc->p_cwd){
                        vput(parent_vnode);
                }
        }
        // KASSERT(0);
        return result;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 *      o EPERM
 *        from is a directory.
 */
int
do_link(const char *from, const char *to)
{
        // NOT_YET_IMPLEMENTED("VFS: do_link"); 

        vnode_t *child_vnode = NULL;
        vnode_t *parent_vnode = NULL;
        size_t namelen;
        const char * buf;
        int value = 0;
        if(strlen(to) > NAME_LEN){
                return -ENAMETOOLONG;
        }
        int pre_ref = curproc->p_cwd->vn_refcount;

        int result1 = open_namev(from, O_RDWR, &child_vnode, curproc->p_cwd);
        int result2 = dir_namev(to, &namelen, &buf, curproc->p_cwd, &parent_vnode);

        int after_ref = curproc->p_cwd->vn_refcount;

        if((parent_vnode!=curproc->p_cwd) && (result2 >= 0)){
                vput(parent_vnode);
        }

        if(result1 >= 0){
                if(S_ISDIR(child_vnode->vn_mode)){
                        value = -EPERM;  
                        vput(child_vnode);
                        return value;
                }
                vput(child_vnode);
        }

        if((result1 < 0) || (result2 < 0)){
                return -ENOTDIR;
        }else{
                value = parent_vnode->vn_ops->link(child_vnode, parent_vnode, buf, namelen);
        }
        
        return value;
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
        int result1 = do_link(newname, oldname);
        int result2 = 0;
        if(result1 < 0){
                do_unlink(newname);
        }
        else{
                result2 = do_unlink(oldname);
        }
        return MIN(result1, result2);

}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{

        int result = 1;
        if(strcmp(path, "/") == 0){
                if(curproc->p_cwd != vfs_root_vn){
                        vput(curproc->p_cwd);
                        curproc->p_cwd = vfs_root_vn;
                }
                return result;
        }
        size_t namelen;
        const char * buf;
        vnode_t *base = curproc->p_cwd;//curent path inode
        vnode_t *parent_vnode = NULL;
        if(dir_namev(path, &namelen, &buf, base, &parent_vnode) > 0){//no such dir
                vnode_t *child_vnode = NULL;
                result = lookup(parent_vnode, (const char *) buf, namelen, &child_vnode);
                if(result >= 0){
                        if(!S_ISDIR(child_vnode->vn_mode)){
                               if(parent_vnode != curproc->p_cwd){
                                        vput(parent_vnode);
                                } 
                                vput(child_vnode);
                                result = -ENOTDIR;
                        }else{
                                result = 0;
                                if(parent_vnode != curproc->p_cwd){
                                        vput(parent_vnode);
                                }
                                vput(curproc->p_cwd);
                                curproc->p_cwd = child_vnode;
                        }
                }else{
                        result = -ENOENT;
                        if(parent_vnode != curproc->p_cwd){
                                vput(parent_vnode);
                        }
                }

        }
        
        return result;
}

/* Call the readdir vn_op on the given fd, filling in the given dirent_t*.
 * If the readdir vn_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
        if((fd < 0)){
                return -EBADF;
        }
        file_t* my_file = fget(fd);
        if (my_file == NULL){
                return -EBADF;
        }else if(!S_ISDIR(my_file->f_vnode->vn_mode)){
                fput(my_file);
                return -ENOTDIR;
        }
        fput(my_file);

        if(!curproc->p_cwd->vn_ops->readdir){

                return -ENOTDIR;
        }

        int result = curproc->p_cwd->vn_ops->readdir(curproc->p_files[fd]->f_vnode, curproc->p_files[fd]->f_pos, dirp);

        if(result > 0){
                result = do_lseek(fd, result, SEEK_CUR);
                if (result < 0){
                        return result;
                }
                return sizeof(*dirp);
        }
        return 0;
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
        // NOT_YET_IMPLEMENTED("VFS: do_lseek");
        if (fd <0 || fd >= NFILES || curproc->p_files[fd]==NULL){
                return -EBADF;
        }

        if (fd <0 || fd >= NFILES || curproc->p_files[fd]==NULL){
                return -EBADF;
        }

        file_t *file_desc = curproc->p_files[fd];

        int future_pos = file_desc->f_pos;

        if(whence == SEEK_CUR){
                future_pos += offset;
        }
        else if(whence == SEEK_SET){
                future_pos = offset;
        }
        else if(whence == SEEK_END){
                future_pos = file_desc->f_vnode->vn_len + offset;
        }
        else{
                return -EINVAL;
        }
        return future_pos < 0 ? -EINVAL : (file_desc->f_pos = future_pos);
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o EINVAL
 *        path is an empty string.
 */
int
do_stat(const char *path, struct stat *buf)
{

        int result = 0;
        size_t namelen;
        const char * buff;
        vnode_t *base = curproc->p_cwd;//curent path inode
        vnode_t *parent_vnode = NULL;
        if(strlen(path) == 0){
                return -EINVAL;
        }
        if((result = dir_namev(path, &namelen, &buff, base, &parent_vnode)) > 0){//no such dir
                vnode_t *child_vnode = NULL;
                if(namelen != 0){
                        result = lookup(parent_vnode, (const char *) buff, namelen, &child_vnode);

                        if(result < 0){
                                result = -ENOENT;
                        }else{
                                vput(child_vnode);
                                child_vnode->vn_ops->stat(child_vnode, buf);
                        }
                }else{
                        parent_vnode->vn_ops->stat(parent_vnode, buf);
                }
                
                if((path[0] == '/') && (namelen == (strlen(path)-1))){
                        result = 0;

                        return result;
                }else if((parent_vnode != curproc->p_cwd)){
                        vput(parent_vnode);
                }
        }
        if(result >= 0){
                result = 0;
        }

        return result;


}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
