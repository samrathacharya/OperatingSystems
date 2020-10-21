#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>

/*
 * Add your file-related functions here ...
 */

 //Called when initialising a process - both proc_create
 //and runprogram

//Helper to connect stdin to std out, could be generalised
int initialise_file(char *filename, int flags, mode_t mode, int *file_descriptor)
{
	//Initialise file descriptor and open file to not found 
    int fd_index = -1, oft_index =-1, i;
    struct vnode* vn;
    int result;
    result = vfs_open(filename, flags, mode, &vn);
    if (result){
        return result;
    }

    //Get the file descriptor table for proess
    struct file_descriptor_table *fd_table = curproc->fd_table;

    //Lock global open filetable
    lock_acquire(of_table->table_lock);

    //Find index in open file table
    for (i=0; i< OPEN_MAX; i++){
        if (of_table->files[i]==NULL){
            oft_index = i;
            break;
        }
    }

    //Find index in file descriptor table
    for (i=0; i<OPEN_MAX; i++){
        if (fd_table->entries[i] == EMPTY){
            fd_index = i;
            break;
        }
    }

    //If not found, then return error
    //Need to close node and release lock 
    if (fd_index==-1 || oft_index==-1){
        vfs_close(vn);
        lock_release(of_table->table_lock);
        return EMFILE;
    }

    //If not,create file descriptor entry as being
    //the index in the open file 
    fd_table->entries[fd_index] = oft_index;

    //Create table entry and add to table
    struct open_file *new_entry = kmalloc(sizeof(struct open_file));
    //return memory error if new entry not possible
    if (new_entry == NULL){
        vfs_close(vn);
        lock_release(of_table->table_lock);
        return ENOMEM;
    }

    new_entry->offset = 0;
    new_entry->flag = flags;
    new_entry->v_node = vn;
    new_entry->count = 1;
    new_entry->name = filename;
    of_table->files[oft_index] = new_entry;

    //Unlock table
    lock_release(of_table->table_lock);
    *file_descriptor = fd_index;
    return 0;

}

int initialise(){
    int i;
    /* OPEN FILE TABLE */
    //Initialise open file table
    if (of_table == NULL){
        of_table = kmalloc(sizeof(struct open_file_table));
        
        //TODO: Return memory error code if it doesnt malloc - is this right?
        //https://piazza.com/class/k6ll17ogrxl56c?cid=274
        if (of_table == NULL){
            return ENOMEM;
        }

        //Create lock
        struct lock *table_lock  = lock_create("of_table_lock");
        if (table_lock == NULL){
            return ENOMEM;
        }

        //Assign
        of_table->table_lock = table_lock;

        //Initialise files to null
        for (i=0; i<OPEN_MAX; i++){
            of_table->files[i] = NULL;
        }
    }

  

    /* PER PROCESS FILE DESCRIPTOR TABLE */
    // curproc is defined in proc.h and current.h

    //Make fd table for process
    curproc->fd_table = kmalloc(sizeof(struct file_descriptor_table));
    if (curproc->fd_table == NULL){
        return ENOMEM;
    }

    //init all entries as empty
    for (i=0; i<OPEN_MAX; i++){
        curproc->fd_table->entries[i] = EMPTY;
    }

    //Connect stdout and stderr and stdin

    char c1[] = "con:";
    char c2[] = "con:";
    char c3[] = "con:";
    int r1,r2,r3, fd;

     //For stdin - needed to pass write
    r3 = initialise_file(c3,O_WRONLY,0,&fd);
    if (r3){
        //Should table be freed/
        return r3;
    }

    //For stdout
    r1 = initialise_file(c1,O_WRONLY,0,&fd);
    if (r1){
        //Should table be freed/
        return r1;
    }

    //For stderr
    r2 = initialise_file(c2,O_WRONLY,0,&fd);
    if (r2){
        //Should table be freed/
        return r2;
    }

    //
    return 0;
}

//Write - seems to be working for now
int sys_write(int fd, userptr_t buffer, size_t n_bytes, int *bytesWritten){
    //Check for valid fd or return error
    if (fd<0 || fd>= OPEN_MAX){
        return EBADF;
    }
    //Get file, check for validity
    int entry = curproc->fd_table->entries[fd];
    if (entry < 0 || entry >= OPEN_MAX){
        return EBADF;
    }

    //Lock table
    lock_acquire(of_table->table_lock);

    //Check to see if file exists (not null) AND
    //if it has permission to be written to
    struct open_file *file_chosen = of_table->files[entry];

    if (file_chosen == NULL){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    //Use mask to see if file can be written to
    if ((O_ACCMODE & file_chosen->flag) == O_RDONLY){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    int result;
    //UIO.H - Abstraction encapsulating memory block
    struct uio uio_temp;
    //IOVEC.H - used in the readv/writev scatter/gather I/O calls
    struct iovec io_temp;

    //Get uio with write ready
    uio_uinit(&io_temp, &uio_temp, buffer, n_bytes, file_chosen->offset, UIO_WRITE);
    //Vnode to write to
    struct vnode *v_node = file_chosen->v_node;

    //Write vnode
    result = VOP_WRITE(v_node, &uio_temp);
    if (result){
        lock_release(of_table->table_lock);
        return result;
    }

    //Get number of bytes written
    // = final offset - initial offset 
    *bytesWritten = uio_temp.uio_offset - file_chosen->offset;
    
    //Update offset
    file_chosen->offset = uio_temp.uio_offset;

    //Remove lovk
    lock_release(of_table->table_lock);

    return 0;
}

//Open 
int sys_open(userptr_t filename, int flags, int mode, int *file_descriptor){   
    int result;
    char safe_filename[PATH_MAX];

    // Copy file name safely
    result = copyinstr(filename,safe_filename,PATH_MAX,NULL);
    if (result){
        return result;
    }

    result = initialise_file(safe_filename,flags,mode,file_descriptor);
    
    //Need to debug
    if(result){
        return result;
    }
    return 0;
}

//Close
int sys_close(int fd){

    //Check to make sure that the fd is valid
    if (fd > OPEN_MAX || fd <0){
        return EBADF;
    }

    //Make sure entry in file descriptor table is valid
    int entry_num = curproc->fd_table->entries[fd];
    if (entry_num < 0 || entry_num > OPEN_MAX){
        return EBADF;
    }

    //Lock file table
    lock_acquire(of_table->table_lock);

    //Get file pointer, make sure valid
    struct open_file *chosen_file = of_table->files[entry_num];
    if (chosen_file == NULL){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    // Close file
    curproc->fd_table->entries[entry_num] = EMPTY;

    //Remove references
    if (chosen_file->count > 1){
        chosen_file->count = chosen_file->count - 1;
    }else{
        vfs_close(chosen_file->v_node);
        kfree(chosen_file);
        of_table->files[entry_num] = NULL;
    }

    //Release lock
    lock_release(of_table->table_lock);

    return 0;
}

//Read - not implemented, just added stub for debugging purposes
int sys_read(int fd, userptr_t buffer, size_t buf_len, int *bytesRead){
    //Check for valid fd
    if (fd >= OPEN_MAX || fd < 0){
        return EBADF;
    }

    //Valid file check
    int entry_num = curproc->fd_table->entries[fd];
    if (entry_num < 0 || entry_num >= OPEN_MAX){
        return EBADF;
    }

    //lock
    lock_acquire(of_table->table_lock);

    //Get file , ensure valid
    struct open_file *chosen_file = of_table->files[entry_num];
    if (chosen_file == NULL){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    //See if it can be read
    if ((chosen_file->flag & O_ACCMODE) == O_WRONLY){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    //Set up uio
    int result;
    struct uio uio_temp;
    struct iovec io_temp;
    struct vnode *v_node = chosen_file->v_node;
    uio_uinit(&io_temp, &uio_temp, buffer, buf_len, chosen_file->offset, UIO_READ);


    //Send to VFS
    result = VOP_READ(v_node,&uio_temp);

    //If err, return err
    if (result){
        lock_release(of_table->table_lock);
        return result;
    }

    //Update offsets and values
    *bytesRead = uio_temp.uio_offset - chosen_file->offset;    

    chosen_file->offset = uio_temp.uio_offset;

    // Release lock
    lock_release(of_table->table_lock);

    return 0;
}

//dup2
int sys_dup2(int old_fd, int new_fd, int *retfd) {  
    //Check for valid fd
    if (old_fd >= OPEN_MAX || old_fd < 0 || new_fd >= OPEN_MAX || new_fd < 0){
        return EBADF;
    }


    // do nothing if old_fd and new_fd are the same
    if (old_fd != new_fd) {

        //Valid file check
        int entry_num = curproc->fd_table->entries[old_fd];
        if (entry_num < 0 || entry_num >= OPEN_MAX){
            return EBADF;
        }

        //lock
        lock_acquire(of_table->table_lock);

        //Get file, ensure valid
        struct open_file *chosen_file = of_table->files[entry_num];
        if (chosen_file == NULL){
            lock_release(of_table->table_lock);
            return EBADF;
        }

        // check if new_fd is an existing open file
        int new_fd_entry_num = curproc->fd_table->entries[new_fd];
        struct open_file *new_fd_file = of_table->files[new_fd_entry_num];
        if (new_fd_file != NULL) {
            int close = sys_close(new_fd);
            // if close() encounted an error
            if (close != 0) {
                lock_release(of_table->table_lock);
                return close;
            }
        }

        // initialise new_fd in per process fd table
        // then point this to the same entry in the open file table as the old_fd
        // increment count
        curproc->fd_table->entries[new_fd] = entry_num;
        chosen_file->count++;


        lock_release(of_table->table_lock);
    }
    *retfd = new_fd;

    return 0;
}


//lseek
off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval64) {
    //Check for valid fd
    if (fd >= OPEN_MAX || fd < 0){
        return EBADF;
    }

    //Valid file check
    int entry_num = curproc->fd_table->entries[fd];
    if (entry_num < 0 || entry_num >= OPEN_MAX){
        return EBADF;
    }

    //lock
    lock_acquire(of_table->table_lock);

    //Get file, ensure valid
    struct open_file *chosen_file = of_table->files[entry_num];
    if (chosen_file == NULL){
        lock_release(of_table->table_lock);
        return EBADF;
    }

    
    struct vnode *v_node = chosen_file->v_node;
    // check if seekable
    if (!VOP_ISSEEKABLE(v_node)) {
        lock_release(of_table->table_lock);
        return ESPIPE;
    }

    // get file size information
    struct stat stat;
    VOP_STAT(v_node, &stat);

    off_t oldOffset = chosen_file->offset;

    off_t offset;

    if (whence == SEEK_SET) {
        offset = pos;
        chosen_file->offset = offset;

    }
    else if (whence == SEEK_CUR) {
        offset = chosen_file->offset + pos;
        chosen_file->offset = offset;
    }
    else if (whence == SEEK_END) {
        offset = stat.st_size + pos;
        chosen_file->offset = offset;
    }
    else {
        lock_release(of_table->table_lock);
        return EINVAL;
    }
    
    // check if offset < 0
    if (offset < 0) {
        chosen_file->offset = oldOffset;
        lock_release(of_table->table_lock);
        return EINVAL;
    }

    *retval64 = offset;
    lock_release(of_table->table_lock);
    return 0;
}