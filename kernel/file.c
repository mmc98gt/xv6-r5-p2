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
  struct vma vmas[VMA_MAX];
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
  for(v = vma_list.vmas; v < vma_list.vmas + VMA_MAX; v++){
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
getFreeVMA()
{
  //buscar una vma libre
  acquire(&vma_list.lock);
  for(int i = 0;i<VMA_MAX;i++)
  {
    if(vma_list.vmas[i].use == 0)
    {
      vma_list.vmas[i].use = 1;
      release(&vma_list.lock);
      return &vma_list.vmas[i]; 
    }
  }
  release(&vma_list.lock);
  return (struct vma *) 0;
}

void*
mmap(void *addr, uint64 length, int prot, int flag, int fd, int offset)
{

  //test
  printf("entra a mmap \n");
  printf("addr: %d, leng: %d, prot: %d, flag: %d, fd: %d, off: %d\n\n",addr,length,prot,flag,fd,offset);

  //obtencion del proceso actual
  struct proc *p = myproc();

  //comprobar flags:
  if(flag == MAP_SHARED && (p->ofile[fd]->writable !=1 || p->ofile[fd]->readable !=1))
    return MAP_FAILED;
  
  //comprobar si hay mma libre;
  acquire(&p->lock);
  if(p->numVmas == VMA_MAX){
    release(&p->lock);
    return MAP_FAILED;
  }

  //declaracion de variables aux
  uint64 p_size;
  struct vma *vma;
  struct vma *actual;
  struct vma *anterior;

  //redondeo de la memoria y actualizar variables aux;
  p_size = PGROUNDUP(length);
  vma = 0; //0 es que no hay vma
  actual = p->vmas;
  anterior = 0; //inicialmente no hay vma anteriores

  //recoriendo las vmas de los procesos
  int i = 0;
  for(i=0;i<=VMA_PROCESS;i++)
  {
    //el proceso no tiene vmas asociadas
    if(actual==0)
    {
      //si hay una vma anterior y esta en una posicion no valida
      if(((anterior != 0) && (anterior->vm_end + p_size) > TOP_ADDRESS) || ((anterior == 0) && START_ADDRESS + p_size > TOP_ADDRESS))
        return MAP_FAILED;

      //no quedan vmas libres
      if((vma = getFreeVMA()) == (struct vma *) 0){
        release(&p->lock);
        return MAP_FAILED;
      }

      if(anterior == 0){
        vma->vm_start = START_ADDRESS;
        vma->vm_end = START_ADDRESS + p_size;
      }else{
        anterior->vm_next = vma;
        vma->vm_start = anterior->vm_end;
        vma->vm_end =  anterior->vm_end + p_size;
      }
      vma->vm_next = 0;
      goto alloc;

    }else if((anterior != 0) && anterior->vm_end + p_size <= actual->vm_start){ //hay una vma anterior y valid
      
      //buscar una vma libre
      if((vma = getFreeVMA()) == (struct vma *) 0){
        release(&p->lock);
        return MAP_FAILED;
      }
      
      //si hay una libre, actualizamos las variables del bucle y vamos a la parte de alloc
      anterior->vm_next = vma;
      vma->vm_next = actual;
      vma->vm_start = anterior->vm_end;
      vma->vm_end = anterior->vm_end + p_size;
      goto alloc;

    }

    //actualizando variables del bucle
    anterior = actual;
    actual = actual->vm_next;

  }

  //no se puede reservar la vma
  return MAP_FAILED;

  alloc:
    vma->vm_len = length;
    vma->vm_prot = prot;
    vma->vm_file = p->ofile[fd];
    vma->vm_flags = flag;
    vma->vm_firstDir = vma->vm_start;
    vma->vm_offset = 0;

  p->ofile[fd]->ref++;
  if(!p->numVmas)
    p->vmas = vma;
  p->numVmas++;

  release(&p->lock);
  return (void *)vma->vm_start;  

}

int
nunmap(uint64 addr, uint64 length)
{
  printf("nunpad entra\n");

  return 0;
}