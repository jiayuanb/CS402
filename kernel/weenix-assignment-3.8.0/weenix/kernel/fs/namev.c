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

#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        if(!dir->vn_ops->lookup){
                 return -ENOTDIR;
        }
        int res = dir->vn_ops->lookup(dir, name, len, result);
        
        // NOT_YET_IMPLEMENTED("VFS: lookup");
        return res;
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
        int result = 0;
        if(*pathname == '/'){
                 *res_vnode = vfs_root_vn;
                char * head = (char *)pathname+1;
                if(!(*head)){
                        *name = (char *)pathname;
                        *namelen = 0;
                        return 1;
                }
                char * anchor = strchr(head, '/');
                vnode_t * child_vnode;
                char buf[256];
                int lookup_res;
 
                vnode_t *res_res[20];
                for(int j=0; j<20; j++){
                        res_res[j] = NULL;
                }
                int i=0;
                while(anchor != NULL){
                         *namelen = strlen(head) - strlen(anchor);
                        strncpy(buf, head, *namelen);
                        buf[*namelen] = '\0';
 
                        vnode_t *next_vnode;
                        int lookup_res = lookup(*res_vnode, buf, *namelen, &next_vnode);

                        head = anchor+1;
                        anchor = strchr(head, '/');
                        
                        if((anchor != NULL) && (lookup_res >= 0)){
                                res_res[i] = next_vnode;
                                i++;
                        }
                        *res_vnode = next_vnode;
                        if((lookup_res < 0)){
                                 // vput(*res_vnode);
                                return -ENOENT;
                        }
                        if(((*res_vnode)->vn_mode == S_IFREG)){

                                vput(*res_vnode);
                                return -ENOTDIR;
                        }
                       
                }

                int t = 0;
                while(res_res[t]){
              
                        vput(res_res[t]);
                        t++;
                }


                *namelen = strlen(head);
                
                *name = (char *)head;
                result = 1;

        }else{


                *res_vnode = base;
                char * head = (char *)pathname;
                char * anchor = strchr(head, '/');
                vnode_t * child_vnode;
                char buf[256];
                int lookup_res;

                vnode_t *res_res[40];
                for(int j=0; j<40; j++){
                        res_res[j] = NULL;
                }
                int i=0;
                while(anchor != NULL){
                        *namelen = strlen(head) - strlen(anchor);
                        strncpy(buf, head, *namelen);
                        buf[*namelen] = '\0';
                        int lookup_res = lookup(*res_vnode, buf, *namelen, res_vnode);
                        do{
                                head = anchor+1;
                                anchor = strchr(head, '/');
                        }while(anchor && (*head == '/'));
                        
                        
                        if((anchor != NULL) && (lookup_res >= 0)){
                                res_res[i] = *res_vnode;
                                i++;
                        }
                        if((lookup_res < 0)){
                                int t = 0;
                                while(res_res[t]){
                                        vput(res_res[t]);
                                        t++;
                                }
                                return -ENOENT;
                        }
                        if(((*res_vnode)->vn_mode == S_IFREG)){
                         
                        
                                int t = 0;
                                while(res_res[t]){
                                        vput(res_res[t]);
                                        t++;
                                }
                                vput(*res_vnode);
                                return -ENOTDIR;
                        }
                        if((anchor == NULL) && (*res_vnode == curproc->p_cwd)){
                                vput(*res_vnode);
                        }
                }

                int t = 0;
                while(res_res[t]){
                        
                        
                        vput(res_res[t]);
                        t++;
                }
                *namelen = strlen(head);
                *name = (char *)head;
                result = 1;
                
        }



        // NOT_YET_IMPLEMENTED("VFS: dir_namev");
        return result;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fcntl.h>.  If the O_CREAT flag is specified and the file does
 * not exist, call create() in the parent directory vnode. However, if the
 * parent directory itself does not exist, this function should fail - in all
 * cases, no files or directories other than the one at the very end of the path
 * should be created.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{


        size_t namelen;
        const char * buf;
        vnode_t *child_vnode = NULL;
        int result;
        result = dir_namev(pathname, &namelen, &buf, base, res_vnode);
        if(result >= 0){

                if(!namelen){
                        return result;
                }

                if(lookup(*res_vnode, (const char *) buf, namelen, &child_vnode) < 0){
                        int new_flag = flag >> 8;
                       
                       
                        if(new_flag != 1){
                                if((pathname[0] == '/') && (namelen == (strlen(pathname)-1))){
                                        *res_vnode = *res_vnode;
                                }else if(*res_vnode != curproc->p_cwd){
                                        vput(*res_vnode);
                                }
                                return -ENOENT;
                        }
                        int create_result = vfs_root_vn->vn_ops->create(*res_vnode, (const char *) buf, namelen, &child_vnode);
                        if(create_result < 0){
                                result = create_result;
                        }

                        if((pathname[0] == '/') && (namelen == (strlen(pathname)-1))){
                                *res_vnode = *res_vnode;
                        }else if(*res_vnode != curproc->p_cwd){
                                vput(*res_vnode);
                        }
                        *res_vnode = child_vnode;

                }
                else{

                        if((pathname[0] == '/') && (namelen == (strlen(pathname)-1))){
                        
                                *res_vnode = *res_vnode;
                        }
                        else if(*res_vnode != curproc->p_cwd){
                       
                       
                                vput(*res_vnode);
                        }
                        *res_vnode = child_vnode;

                }
        

        }

        return result;

}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
