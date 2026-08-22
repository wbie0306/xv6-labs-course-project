#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 base, maskaddr;
  int len;
  uint mask = 0;
  pte_t *pte;
  struct proc *p = myproc();

  if(argaddr(0, &base) < 0 || argint(1, &len) < 0 ||
     argaddr(2, &maskaddr) < 0)
    return -1;
  if(len < 0 || len > 32)
    return -1;

  for(int i = 0; i < len; i++){
    pte = walk(p->pagetable, base + (uint64)i * PGSIZE, 0);
    if(pte == 0 || (*pte & PTE_V) == 0)
      return -1;
    if(*pte & PTE_A){
      mask |= 1U << i;
      *pte &= ~PTE_A;
    }
  }

  // Ensure later references reload translations and update PTE_A again.
  sfence_vma();
  if(copyout(p->pagetable, maskaddr, (char *)&mask, sizeof(mask)) < 0)
    return -1;
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
