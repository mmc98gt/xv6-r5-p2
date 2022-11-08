//campos necesarios para mmap
#define PROT_READ (1L << 1)
#define PROT_WRITE (1L << 2)
#define PROT_READ_WRITE ((1L << 1)|(1L << 2))

//flags para mmap
#define MAP_PRIVATE 1
#define MAP_SHARED 2

#define VMA_MAX 32

//Comienzo de la zona mapeable
#define START_ADDRESS 0x2000000000  

//direcion maxima de mapeo
#define TOP_ADDRESS 0x3FFFFFDFFF 

//fallo en mmap
#define MAP_FAILED ((char *) -1)


// the struct of a vma.
struct vma {
// vma start address
    uint64  vm_start;
//vma offset
    uint64  vm_offset;
// vma end address
    uint64  vm_end;
// vma lenght
    uint64 vm_len;
// vma flags
    uint64 vm_flags;
// vma proctection
    uint64 vm_prot;
// vma ref
    uint64 vm_ref;
// vma firts dir
    uint64 vm_firstDir;
// vma's next vma
    struct vma *vm_next;
// vma's file
    struct file *vm_file;
// vma's use or not
    int use;
};

