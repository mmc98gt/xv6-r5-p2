//campos necesarios para mmap
#define PROT_READ (1L << 1)
#define PROT_WRITE (1L << 2)
#define PROT_READ_WRITE ((1L << 1)|(1L << 2))

//flags para mmap
#define MAP_PRIVATE 1
#define MAP_SHARED 2

// the struct of a vma.
struct vma {
// vma start address
    void * vm_start;
// vma end address
    void * vm_end;
// vma lenght
    uint64 vm_len;
// vma flags
    uint64 vm_flags;
// vma proctection
    uint64 vm_prot;
// vma ref
    uint64 vm_ref;
// vma's next vma
    struct vma *vm_next;
// vma's file
    struct file *vm_file;
};

