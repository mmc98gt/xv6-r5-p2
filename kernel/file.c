//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "vma.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

// Struct to create a lock for vmalist
struct {
  struct spinlock lock;
  struct vma vmas[NPROC*2];
} vma_list;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}
void vmalistinit(void)
{
  initlock(&vma_list.lock, "vma_list");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

struct vma*
vmaalloc(void)
{
  struct vma *v;

  acquire(&vma_list.lock);
  for(v = vma_list.vmas; v < vma_list.vmas + NPROC*2; v++){
    if(v->vm_ref == 0){
      v->vm_ref = 1;
      release(&vma_list.lock);
      return v;
    }
  }
  release(&vma_list.lock);
  return 0;
}
struct vma*
checkaddr(void* addr)
{
  // struct proc *p = myproc();
  struct vma *v;
  acquire(&vma_list.lock);
  for(v = vma_list.vmas; v < vma_list.vmas + NPROC*2; v++){
    if(v->vm_ref == 1 && v->vm_start <= addr && v->vm_end >= addr){
      release(&vma_list.lock);
      return v;
    }
  }
  release(&vma_list.lock);
  return 0;
}
int
mmap(int mode ,void *addr, uint64 length, int prot, int flag, int fd, int off)
{
  
  

  //guardar informacion en el VMA
  //instance of VMA
  struct vma *vma;
  p->vmas[p->numVmas];
  p->numVmas++;
  //instance of file
  struct file *f;
  //get file from fd
  if(fd <0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
    return -1;
  //puting args in vma
  if(off < 0)
    return -1;
  f->off = off;
  if((vma = vmaalloc()) == 0)
    return -1;

  vma->vm_start = addr;
  vma->vm_end = (struct vma*)addr + length;
  vma->vm_prot = prot;
  vma->vm_flags = flag;
  vma->vm_file = f;
  // increment ref count
  f->ref++;
  vma->vm_next = 0;

  // calculing addr
  struct proc* p = myproc();
  if(!mode){
    if(p ->numVmas){
      uint64 aux = p->trapframe + sizeof(struct trapframe);
      addr = (void*) aux;  
    }
    addr = p->lastDirVma;
  }

  
  return 1;
}

int
nunmap(uint64 addr, uint64 length)
{
  return 0;
}