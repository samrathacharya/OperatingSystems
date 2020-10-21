/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>

/*
 * Put your function declarations and data types here ...
 */

#define EMPTY -1

//For testing purposes to ignore unused params
#define UNUSED(x) (void)(x)

// Open file entry
struct open_file {
    struct vnode *v_node;  //vnode pointer file points to
    int flag; //if it is read/write or both
    off_t offset; //offset within file
    int count;  //no. of times referenced
    char* name;
};

//Open file table
struct open_file_table {
    struct lock *table_lock; 
    struct open_file *files[OPEN_MAX];  //Array of open files
};

//Pointer to open file table
struct open_file_table *of_table;

// File descriptor entry - unique per process
struct file_descriptor_table {
    //Index = file descripter number
    //Value = index of entry in the open_file_table
    int entries[OPEN_MAX];
};


//Helpers
int initialise_file(char *filename, int flags, mode_t mode, int *file_descriptor);

int initialise(void);

//Syscall funtions - Should move to different header file
// int sys_open(userptr_t filename, int flags, int mode, int *file_descriptor);
// int sys_write(int fd, userptr_t buffer, size_t n_bytes, int *size);
// int sys_read(int fd, userptr_t buffer, size_t buf_len, int *size);
// int sys_lseek(int fd, off_t pos, int switch_code, off_t *retval);
// int sys_dup2(int fd, int new_fd, int *retval);

#endif /* _FILE_H_ */
