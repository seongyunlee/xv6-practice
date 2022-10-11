#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
int uproc_start_time;

double weight[40]={
    88817, 71054, 56843, 45474, 36379, 29103, 23283, 18626, 14901, 11920, 9536, 7629, 6103, 4882, 3906, 3124, 2500, 2000, 1600, 1280, 1024, 819, 655, 524, 419, 335, 268, 214, 171, 137, 109, 87, 70, 56, 45, 36, 28, 23, 18, 14
};

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void add_vruntime(uint* p, uint elapsed);
int compare_vruntime(uint* a, uint* b);
void set_wokenup_vruntime(uint *woken,uint *min, int nice);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->nice = 20;
  memset(p->vruntime,0,sizeof(uint)*4);
  memset(p->scaled_runtime,0,sizeof(uint)*4);
  memset(p->runtime,0,sizeof(uint)*4);
  p->time_slice=0;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  if(curproc->parent->state==SLEEPING && curproc->parent->chan==curproc->parent){
    wakeup1(curproc->parent);
    //set woken process's vruntime
    struct proc *min_p; // process which has minimum vruntime
    min_p=curproc->parent;
    struct proc *pp;
    for(pp= ptable.proc; pp<&ptable.proc[NPROC]; pp++){
      if(pp->state != RUNNABLE)
        continue;
      if(compare_vruntime(min_p->vruntime,pp->vruntime))
        min_p=pp;
    }
    set_wokenup_vruntime(curproc->parent->vruntime,min_p->vruntime,curproc->parent->nice);
  }

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
 //
  add_vruntime(p->vruntime,(uint)(((double)(ticks-uproc_start_time))*(1024/weight[p->nice]*1000))); //
  add_vruntime(p->scaled_runtime,(uint)(((double)(ticks-uproc_start_time))*(1000/weight[p->nice]))); //
  add_vruntime(p->runtime,(ticks-uproc_start_time)*1000);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

void sys_sleepEnd(struct proc *p){
  
  acquire(&ptable.lock);

  struct proc *min_p; // process which has minimum vruntime
  min_p=p;
  struct proc *pp;
  for(pp= ptable.proc; pp<&ptable.proc[NPROC]; pp++){
    if(pp->state != RUNNABLE)
      continue;
    if(compare_vruntime(min_p->vruntime,pp->vruntime))
      min_p=pp;
  }
  set_wokenup_vruntime(p->vruntime,min_p->vruntime,p->nice);

  release(&ptable.lock);
  return;
}

void sys_sleepStart(struct proc *p){
  
  add_vruntime(p->vruntime,(uint)(((double)(ticks-uproc_start_time))*(1024/weight[p->nice]*1000))); //
  add_vruntime(p->scaled_runtime,(uint)(((double)(ticks-uproc_start_time))*(1000/weight[p->nice]))); //
  add_vruntime(p->runtime,(uint)(ticks-uproc_start_time)*1000);
  return;
}



// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      struct proc *min_p; // process which has minimum vruntime
      double total_weight=0;
      min_p=p;
      struct proc *pp;
      for(pp= ptable.proc; pp<&ptable.proc[NPROC]; pp++){
        if(pp->state != RUNNABLE)
          continue;
        total_weight+=weight[pp->nice];
        if(compare_vruntime(min_p->vruntime,pp->vruntime))
          min_p=pp;
      }
      //cprintf("total weight of runnable %d\n",(int)total_weight);
      min_p->time_slice=(int)(10*(weight[min_p->nice])/total_weight)+1;
      //cprintf("Give %d time slice to %d\n",min_p->time_slice,min_p->pid);

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      uproc_start_time= ticks;       //set process start time to current tick;
      c->proc = min_p;
      switchuvm(min_p);
      min_p->state = RUNNING;

      swtch(&(c->scheduler), min_p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  struct proc *p= myproc();
  if(p->time_slice<=(ticks-uproc_start_time)){
    p->state = RUNNABLE;
    add_vruntime(p->vruntime,(uint)(((double)(ticks-uproc_start_time))*(1024/weight[p->nice]*1000))); //
    add_vruntime(p->scaled_runtime,(uint)(((double)(ticks-uproc_start_time))*(1000/weight[p->nice]))); //
    add_vruntime(p->runtime,(ticks-uproc_start_time)*1000);
    sched();
   }
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  sched();
  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
int
getpname(int pid){
  struct proc *p;

  acquire(&ptable.lock);
  for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
    if(p->pid ==pid){
      cprintf("%s\n",p->name);
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
//set woken pointer(4 uint array) to min-unit_vruntime
void set_wokenup_vruntime(uint *woken,uint *min,int nice){
    uint unit_vruntime=(uint)(1024/weight[nice]);
    woken[0]= unit_vruntime<min[0]?min[0]-unit_vruntime:0;
    for(int i=1;i<4;i++){
      woken[i]=min[i];
    }
}
//compare a,b return 1 if a>b else 0
int compare_vruntime(uint* a,uint* b){
   for(int i=3;i>=0;i--){
    if(a[i]>b[i])
        return 1;
    else if(a[i]<b[i])
        return 0;
   }
   return 0;
}
//add elapsed to 4 uint array;
void add_vruntime(uint  *p,uint elapsed){
    for(int i=0;i<4;i++){
        if(999999999-p[i]<elapsed){
            p[i]=elapsed-(999999999-p[i])-1;
            elapsed=1;   
        }
        else{
            p[i]+=elapsed;
            elapsed=0;
        }
    }
}
int getnice(int pid){
    struct proc *p;
    
    acquire(&ptable.lock);
    for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
        if(p->pid == pid){
            int nice=p->nice;
            release(&ptable.lock);
            return nice;
        }
    }
    release(&ptable.lock);
    return -1;  
}
int setnice(int pid,int new_nice){

    if(new_nice<0 || new_nice>39){
      return -1;
    }  
    struct proc *p;

    acquire(&ptable.lock);
    for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
        if(p->pid == pid){
            p->nice=new_nice;
            release(&ptable.lock);
            return 0;
            }
    }
    release(&ptable.lock);
    return -1;  
}
void printUintArrayFormatted(uint *x){
  int start=0;
  int cnt=0;
  for(int i=3;i>=0;i--){
    if(start==0 && x[i]!=0){
      cprintf("%d",x[i]);//assert(x<=INT_MAX);
      uint a=x[i];
      //cnt+=len(str(x[i]))
      if(a==0)
        cnt++;
      while(a>0){
        cnt++;
        a/=10;
      }
      start=1;
    }
    else if(start!=0){
      int zeros=9;
      uint a=x[i];
      //cnt+=len(str(x[i]))
      if(a==0)
        zeros=9;
      while(a>0){
        zeros--;
        a/=10;
      }
      for(;zeros>0;zeros--)
        cprintf("0");
      cprintf("%d",x[i]);
      cnt+=9;
    }
    else if(start==0 &&i==0){
      cprintf("0");
      cnt=1;
    }
  }
  for(int s=0;s<37-cnt;s++)
    cprintf(" ");
}
/*
void printunsignedlonglong(unsigned long long x) {
	char n[19];
	int idx = 0;
	while (1) {
		n[idx++] = (int)(x % 10);
		x = x / 10;
		if (x == 0)
			break;
	}
	for (int i = idx - 1; i >= 0; i--) {
		cprintf("%d", n[i]);
	}
	for (int i = 19 - idx; i > 0; i--) {
		cprintf(" ");
	}
}
*/
void printIntFormatted(int x){
  x<<=1;
  x=(int)((uint)x>>1);
  cprintf("%d",x);
  int k=0;
  if(x==0){
    k=1;
  }
  else{
    while(x>0){
      k++;
      x/=10;
    }
  }
  k=16-k;
  while(k>0){
    cprintf(" ");
    k--;
  }
}
void ps(int pid){
    static char *states[] = {
    [UNUSED]    "UNUSED  ",
    [EMBRYO]    "EMBRYO  ",
    [SLEEPING]  "SLEEPING",
    [RUNNABLE]  "RUNNABLE",
    [RUNNING]   "RUNNING ",
    [ZOMBIE]    "ZOMBIE  "
    };

    struct proc *p;
    
    char s1[21]="                    "; //empty string 20;

    acquire(&ptable.lock);
    cprintf("name\t\tpid\tstate       priority\t    runtime/weight   %sruntime          %svruntime         %stick %d\n",s1,s1,s1,ticks*1000);
    for(p=ptable.proc;p<&ptable.proc[NPROC];p++){
        if(pid==0 || p->pid == pid){
            if(p->pid!=0){
                cprintf("%s\t\t%d\t%s    ",p->name,p->pid,states[p->state]);
                printIntFormatted(p->nice);//priority
                printUintArrayFormatted(p->scaled_runtime);//runtime/weight (millitick)
                printUintArrayFormatted(p->runtime);
                printUintArrayFormatted(p->vruntime);
                cprintf("\n");
            }
        }
    }
    release(&ptable.lock);
    return;  
}
