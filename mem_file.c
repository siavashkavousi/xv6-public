#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fcntl.h"
#include "fs.h"
#include "file.h"

void
write_to_file(struct proc *p, char *name, char *data, int size) {
    int fd;
    struct file *file;

    if ((fd = open_file(name, O_CREATE | O_RDWR)) < 0)
        panic("save_process: open_file failed\n");
    file = p->ofile[fd];
    if ((filewrite(file, data, size)) != size)
        panic("save_process: filewrite failed\n");
}

static void
write_to_file_pgtable(struct proc *p, char *name, int va) {
    char *mem = copy_pgtable2mem(proc->pgdir, va);
    write_to_file(p, name, mem, PGSIZE);
    kfree(mem);
}

int
write_to_file_pgtables(struct proc *p, char *name) {
    int size = strlen(name), i, counter = 0;
    char final_name[size + 1];

    for (i = 0; i < p->sz; i += PGSIZE) {
        char c = itoa(counter);
        strncat(final_name, name, &c, 1);
        write_to_file_pgtable(p, final_name, i);
        counter++;
    }

    return counter;
}

void
read_from_file(struct proc *p, char *name, void *data, int size) {
    int fd;
    struct file *file;

    if ((fd = open_file(name, O_RDONLY)) < 0)
        panic("load_process: open_file failed\n");
    file = p->ofile[fd];
    if ((fileread(file, (char *) data, size)) != size)
        panic("load_process: fileread failed\n");
}

static char *
read_from_file_pgtable(struct proc *p, char *name) {
    char *mem = kalloc();
    read_from_file(p, name, mem, PGSIZE);
    return mem;
}

void
map_from_file_pgtables(struct proc *current_process, char *name,
                       struct proc *on_resume_process, int process_size) {
    int i, counter = 0, size = strlen(name);
    char final_name[size + 1];

    for (i = 0; i < process_size; i += PGSIZE) {
        char c = itoa(counter);
        strncat(final_name, name, &c, 1);
        char *data = read_from_file_pgtable(current_process, final_name);

        if ((copy_mem2pgtable(on_resume_process->pgdir, (void *) i, data)) < 0)
            panic("load_process: memory to pagetable mapping failed\n");

        counter++;
    }
}