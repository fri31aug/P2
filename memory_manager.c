//
// Created by chang on 10/15/23.
// Edited by Ananya on 11/10/23.
//



#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>

// TODO 1: Define your input parameter (pid - int) here
// Then use module_param to pass them from insmod command line. (--Assignment 2)
int pid = 0;  // Default value 0
module_param(pid, int, 0644);
MODULE_PARM_DESC(pid, "Process ID");

struct task_struct *task = NULL;
// Initialize memory statistics variables
unsigned long total_rss = 0;
unsigned long total_swap = 0;
unsigned long total_wss = 0;

static void parse_vma(void) {
    struct vm_area_struct *vma = NULL;
    struct mm_struct *mm = NULL;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long page;

    if (pid > 0) {
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if (task && task->mm) {
            mm = task->mm;

            // TODO 2: mm_struct to initialize the VMA_ITERATOR (-- Assignment 4)
            // Use mmap_sem to lock and access the VMA
            down_read(&mm->mmap_sem);
            vma = mm->mmap;

            // TODO 3: Iterate through the VMA linked list with for_each_vma (-- Assignment 4)
            for (; vma; vma = vma->vm_next) {

                // TODO 4: Iterate through each page of the VMA
                for (page = vma->vm_start; page < vma->vm_end; page += PAGE_SIZE) {

                    // TODO 5: Use pgd_offset, p4d_offset, pud_offset, pmd_offset, pte_offset_map to get the page table entry
                    pgd = pgd_offset(mm, page);
                    if (pgd_none(*pgd) || pgd_bad(*pgd)) continue;
                    p4d = p4d_offset(pgd, page);
                    if (p4d_none(*p4d) || p4d_bad(*p4d)) continue;
                    pud = pud_offset(p4d, page);
                    if (pud_none(*pud) || pud_bad(*pud)) continue;
                    pmd = pmd_offset(pud, page);
                    if (pmd_none(*pmd) || pmd_bad(*pmd)) continue;
                    pte = pte_offset_map(pmd, page);
                    if (!pte) continue;

                    // TODO 6: use pte_none(pte) to check if the page table entry is valid
                    if (pte_none(*pte)) continue;

                    // TODO 7: use pte_present(pte) to check if the page is in memory, otherwise it is in swap
                    if (pte_present(*pte)) {
                        total_rss++;  // Page is in memory, increment RSS count
                    } else {
                        total_swap++; // Page is in swap, increment swap count
                    }

                    // TODO 8: use pte_young(pte) to check if the page is actively used
                    if (pte_young(*pte)) {
                        total_wss++;
                        // Clear the accessed bit
                        pte = pte_mkold(*pte);
                        set_pte_at(mm, page, pte, *pte);
                    }

                    // Unmap the page table entry
                    pte_unmap(pte);
                }
            }
            // Release the mmap_sem lock
            up_read(&mm->mmap_sem);
        }
    }
}

unsigned long timer_interval_ns = 10e9; // 10 sec timer
static struct hrtimer hr_timer;

enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart) {
    ktime_t currtime, interval;
    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer_for_restart, currtime, interval);
    total_rss = 0;
    total_swap = 0;
    total_wss = 0;
    parse_vma();
    printk("[PID-%i]:[RSS:%lu MB] [Swap:%lu MB] [WSS:%lu MB]\n", pid, total_rss * 4 / 1024, total_swap * 4 / 1024, total_wss * 4 / 1024);
    return HRTIMER_RESTART;
}

int memory_init(void) {
    printk("CSE330 Project 2 Kernel Module Inserted\n");
    ktime_t ktime;
    ktime = ktime_set(0, timer_interval_ns);
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = &timer_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
    return 0;
}

void memory_cleanup(void) {
    int ret;
    ret = hrtimer_cancel(&hr_timer);
    if (ret) printk("HR Timer cancelled ...\n");
    printk("CSE330 Project 2 Kernel Module Removed\n");
}

module_init(memory_init);
module_exit(memory_cleanup);

MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ananya Yadav");
MODULE_DESCRIPTION("CSE330 Project 2 Memory Management\n");
