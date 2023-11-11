//
// Created by chang on 10/15/23.
// Edited by Ananya on 11/10/23.
//



#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

static int pid;
module_param(pid, int, 0644);
MODULE_PARM_DESC(pid, "Process ID");

struct task_struct *task = NULL;
unsigned long total_rss = 0;
unsigned long total_swap = 0;
unsigned long total_wss = 0;

static void parse_vma(void)
{
    struct vm_area_struct *vma = NULL;
    struct mm_struct *mm = NULL;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    unsigned long page;

    if(pid > 0){
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if(task && task->mm){
            mm = task->mm;
            vma = find_vma(mm, 0); // Get the first VMA

            while (vma) {
                for (page = vma->vm_start; page < vma->vm_end; page += PAGE_SIZE) {
                    pgd = pgd_offset(mm, page);
                    if (pgd_none(*pgd) || pgd_bad(*pgd)) continue;

                    p4d = p4d_offset(pgd, page);
                    if (p4d_none(*p4d) || p4d_bad(*p4d)) continue;

                    pud = pud_offset(p4d, page);
                    if (pud_none(*pud) || pud_bad(*pud)) continue;

                    pmd = pmd_offset(pud, page);
                    if (pmd_none(*pmd) || pmd_bad(*pmd)) continue;

                    ptep = pte_offset_map(pmd, page);
                    if (!ptep) continue;
                    pte = *ptep;

                    if (pte_none(pte)) continue;

                    if (pte_present(pte)) {
                        total_rss++;
                        if (pte_young(pte)) {
                            total_wss++;
                            test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)ptep);
                        }
                    } else {
                        total_swap++;
                    }
                    pte_unmap(ptep);
                }
                vma = find_vma(mm, vma->vm_end); // Move to the next VMA
            }
        }
    }
}

unsigned long timer_interval_ns = 10e9; // 10 sec timer
static struct hrtimer hr_timer;

enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart)
{
    ktime_t currtime, interval;
    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer_for_restart, currtime, interval);

    total_rss = 0;
    total_swap = 0;
    total_wss = 0;
    parse_vma();

    printk("[PID-%i]:[RSS:%lu MB] [Swap:%lu MB] [WSS:%lu MB]\n", 
           pid, total_rss * (PAGE_SIZE / 1024) / 1024, 
           total_swap * (PAGE_SIZE / 1024) / 1024, 
           total_wss * (PAGE_SIZE / 1024) / 1024);

    return HRTIMER_RESTART;
}

int memory_init(void)
{
    ktime_t ktime;
    printk("CSE330 Project 2 Kernel Module Inserted\n");
    ktime = ktime_set(0, timer_interval_ns);
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = &timer_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
    return 0;
}

void memory_cleanup(void)
{
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
