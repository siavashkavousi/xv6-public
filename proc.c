#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;

extern void forkret(void);

extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void) {
    initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void) {
    struct proc *p;
    char *sp;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED)
            goto found;
    release(&ptable.lock);
    return 0;

    found:
    p->state = EMBRYO;
    p->pid = nextpid++;
    release(&ptable.lock);

    // Allocate kernel stack.
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe *) sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *) sp = (uint) trapret;

    sp -= sizeof *p->context;
    p->context = (struct context *) sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint) forkret;

    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void) {
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();
    initproc = p;
    if ((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
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

    p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n) {
    uint sz;

    sz = proc->sz;
    if (n > 0) {
        if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
            return -1;
    }
    proc->sz = sz;
    switchuvm(proc);
    return 0;
}

void
suspend_process(char *path) {
    //region work with buffer
//    struct buf *buffer = bread(0, 512);
//
//    procdump();
//
//    memmove(buffer->data, proc, sizeof(*proc));
//    bwrite(buffer);
//    brelse(buffer);
//
//    cprintf("save_process: successful\n");
//    exit();
    //endregion

    int fd;
    struct file *file;

    //region process struct
    write2file(proc, "backup", (char *) proc);
    //endregion

    //region trapframe
//    write2file(proc, "backup_tf", )
    if ((fd = open_file("backup_tf", O_CREATE | O_RDWR)) < 0)
        panic("save_process: backup_tf open_file failed\n");
    file = proc->ofile[fd];
    if ((filewrite(file, (char *) proc->tf, sizeof(struct trapframe))) != sizeof(struct trapframe))
        panic("save_process: backup_tf filewrite failed\n");

    cprintf("trapframe : %d and %d\n", proc->tf->eip, proc->tf->cs);
    //endregion

    //region beta (copy page tables)
//    int i;
//    int counter = 0;
//    for (i = 0; i < proc->sz; i += PGSIZE) {

    char buf[4];
    char *sl = "sia";
    int x = 0;
    char i = (char) (x + 48);

    concat(buf, sl, i);

    char *mem = copy_pgtable2mem(proc->pgdir, 0);
    if ((fd = open_file("backup_mem1", O_CREATE | O_RDWR)) < 0)
        panic("save_process: backup_mem1 open_file failedd\n");
    file = proc->ofile[fd];
    if ((filewrite(file, mem, PGSIZE)) != PGSIZE)
        panic("save_process: backup_mem1 filewrite failed\n");
    kfree(mem);

    mem = copy_pgtable2mem(proc->pgdir, 4096);
    if ((fd = open_file("backup_mem2", O_CREATE | O_RDWR)) < 0)
        panic("save_process: backup_mem2 open_file failedd\n");
    file = proc->ofile[fd];
    if ((filewrite(file, mem, PGSIZE)) != PGSIZE)
        panic("save_process: backup_mem2 filewrite failed\n");
    kfree(mem);

    mem = copy_pgtable2mem(proc->pgdir, 8192);
    if ((fd = open_file("backup_mem3", O_CREATE | O_RDWR)) < 0)
        panic("save_process: backup_mem3 open_file failedd\n");
    file = proc->ofile[fd];
    if ((filewrite(file, mem, PGSIZE)) != PGSIZE)
        panic("save_process: backup_mem3 filewrite failed\n");
    kfree(mem);

//    }
    //endregion

    cprintf("save_process: successful\n");
    exit();

}

void
resume_process(char *path) {
    int fd;
    struct file *file;

    //region process struct
    struct proc p;

    if ((fd = open_file("backup", O_RDONLY)) < 0)
        panic("load_process: backup open_file failed\n");
    file = proc->ofile[fd];
    if ((fileread(file, (char *) &p, sizeof(struct proc))) != sizeof(struct proc))
        panic("load_process: backup fileread failed\n");
    //endregion

    struct proc *ld_proc;
    if ((ld_proc = allocproc()) == 0)
        return;

    if ((ld_proc->pgdir = setupkvm()) == 0)
        panic("load_process: setupkvm\n");

    //region beta (copy page tables)
    char *mem1 = kalloc();
    if ((fd = open_file("backup_mem1", O_RDONLY)) < 0)
        panic("load_process: backup_mem1 open_file failed\n");
    file = proc->ofile[fd];
    if ((fileread(file, mem1, PGSIZE)) != PGSIZE)
        panic("load_process: backup_mem1 fileread failed\n");
    if ((copy_mem2pgtable(ld_proc->pgdir, 0, mem1)) < 0)
        panic("load_process: backup_mem1 memory to pagetable mapping failed\n");

    char *mem2 = kalloc();
    if ((fd = open_file("backup_mem2", O_RDONLY)) < 0)
        panic("load_process: backup_mem2 open_file failed\n");
    file = proc->ofile[fd];
    if ((fileread(file, mem2, PGSIZE)) != PGSIZE)
        panic("load_process: backup_mem2 fileread failed\n");
    if ((copy_mem2pgtable(ld_proc->pgdir, (void *) 4096, mem2)) < 0)
        panic("load_process: backup_mem1 memory to pagetable mapping failed\n");

    char *mem3 = kalloc();
    if ((fd = open_file("backup_mem3", O_RDONLY)) < 0)
        panic("load_process: backup_mem3 open_file failed\n");
    file = proc->ofile[fd];
    if ((fileread(file, mem3, PGSIZE)) != PGSIZE)
        panic("load_process: backup_mem3 fileread failed\n");
    if ((copy_mem2pgtable(ld_proc->pgdir, (void *) 8192, mem3)) < 0)
        panic("load_process: backup_mem1 memory to pagetable mapping failed\n");
    //endregion

//    //region trapframe
//    memset(ld_proc->tf, 0, sizeof(struct trapframe));
//
//    cprintf("trapframe : %d and %d\n", ld_proc->tf->eip, ld_proc->tf->cs);
//
//    if ((fd = open_file("backup_tf", O_RDONLY)) < 0)
//        panic("load_process: backup_tf open_file failed\n");
//    file = proc->ofile[fd];
//    if ((fileread(file, (char *) ld_proc->tf, sizeof(struct trapframe))) != sizeof(struct trapframe))
//        panic("load_process: backup_tf fileread failed\n");
//
//    cprintf("trapframe : %d and %d\n", ld_proc->tf->eip, ld_proc->tf->cs);
//    //endregion

    cprintf("load proc %d\n", ld_proc->sz);
    ld_proc->sz = p.sz;
    cprintf("load proc %d\n", ld_proc->sz);

    int i;
    for (i = 0; i < NOFILE; i++)
        if (p.ofile[i])
            ld_proc->ofile[i] = filedup(p.ofile[i]);
    ld_proc->cwd = idup(p.cwd);

    safestrcpy(ld_proc->name, p.name, sizeof(p.name));

    // lock to force the compiler to emit the np->state write last.
    acquire(&ptable.lock);
    ld_proc->state = RUNNABLE;
    cprintf("process state: RUNNABLE\n");
    release(&ptable.lock);

    cprintf("current proc : %d and loaded process : %d\n", proc->pid, ld_proc->pid);

    //region work with buffer
//    cprintf("I'm here\n");
//    struct buf *buffer = bread(0, 512);
//    struct proc proc1;
//    memmove(&proc1, buffer->data, sizeof(buffer->data));
//
//    struct proc *loaded_process;
//
//    if ((loaded_process = allocproc()) == 0) {
//        cprintf("I'm here1\n");
//        return;
//    }
//
//    if ((loaded_process->pgdir = copyuvm(proc1.pgdir, proc1.sz)) == 0) {
//        kfree(loaded_process->kstack);
//        loaded_process->kstack = 0;
//        loaded_process->state = UNUSED;
//        cprintf("I'm here2\n");
//        return;
//    }
//    loaded_process->sz = proc1.sz;
//    *loaded_process->tf = *proc1.tf;
//
//    // Clear %eax so that fork returns 0 in the child.
//    loaded_process->tf->eax = 0;
//
//    int i;
//    for (i = 0; i < NOFILE; i++)
//        if (proc1.ofile[i])
//            loaded_process->ofile[i] = filedup(proc1.ofile[i]);
//    loaded_process->cwd = idup(proc1.cwd);
//
//    safestrcpy(loaded_process->name, proc1.name, sizeof(proc1.name));
//
//    // lock to force the compiler to emit the np->state write last.
//    acquire(&ptable.lock);
//    loaded_process->state = RUNNABLE;
//    release(&ptable.lock);
    //endregion
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void) {
    int i, pid;
    struct proc *np;

    // Allocate process.
    if ((np = allocproc()) == 0)
        return -1;

    // Copy process state from p.
    if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0) {
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = proc->sz;
    np->parent = proc;
    *np->tf = *proc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for (i = 0; i < NOFILE; i++)
        if (proc->ofile[i])
            np->ofile[i] = filedup(proc->ofile[i]);
    np->cwd = idup(proc->cwd);

    safestrcpy(np->name, proc->name, sizeof(proc->name));

    pid = np->pid;

    // lock to force the compiler to emit the np->state write last.
    acquire(&ptable.lock);
    np->state = RUNNABLE;
    release(&ptable.lock);

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void) {
    struct proc *p;
    int fd;

    if (proc == initproc)
        panic("init exiting");

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++) {
        if (proc->ofile[fd]) {
            fileclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(proc->cwd);
    end_op();
    proc->cwd = 0;

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(proc->parent);

    // Pass abandoned children to init.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == proc) {
            p->parent = initproc;
            if (p->state == ZOMBIE)
                wakeup1(initproc);
        }
    }

    // Jump into the scheduler, never to return.
    proc->state = ZOMBIE;
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void) {
    struct proc *p;
    int havekids, pid;

    acquire(&ptable.lock);
    for (; ;) {
        // Scan through table looking for zombie children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != proc)
                continue;
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.
                pid = p->pid;
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->state = UNUSED;
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || proc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(proc, &ptable.lock);  //DOC: wait-sleep
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
scheduler(void) {
    struct proc *p;

    for (; ;) {
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != RUNNABLE)
                continue;

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            swtch(&cpu->scheduler, proc->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            proc = 0;
        }
        release(&ptable.lock);

    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void) {
    int intena;

    if (!holding(&ptable.lock))
        panic("sched ptable.lock");
    if (cpu->ncli != 1)
        panic("sched locks");
    if (proc->state == RUNNING)
        panic("sched running");
    if (readeflags() & FL_IF)
        panic("sched interruptible");
    intena = cpu->intena;
    swtch(&proc->context, cpu->scheduler);
    cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void) {
    acquire(&ptable.lock);  //DOC: yieldlock
    proc->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void) {
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
sleep(void *chan, struct spinlock *lk) {
    if (proc == 0)
        panic("sleep");

    if (lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) {  //DOC: sleeplock0
        acquire(&ptable.lock);  //DOC: sleeplock1
        release(lk);
    }

    // Go to sleep.
    proc->chan = chan;
    proc->state = SLEEPING;
    sched();

    // Tidy up.
    proc->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock) {  //DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) {
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan) {
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid) {
    struct proc *p;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // Wake process from sleep if necessary.
            if (p->state == SLEEPING)
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
procdump(void) {
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

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%d %s %s", p->pid, state, p->name);
        if (p->state == SLEEPING) {
            getcallerpcs((uint *) p->context->ebp + 2, pc);
            for (i = 0; i < 10 && pc[i] != 0; i++)
                cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}
