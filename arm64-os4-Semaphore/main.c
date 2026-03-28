#include <stdarg.h>
#include <stdint.h>
#include "armv8util.h"
#include "gicv2.h"

#define TRUE  1
#define FALSE 0

#define UART_BASE 0xFE215040U
#define UART_TX   (UART_BASE + 0U*4U)
#define UART_LSR  (UART_BASE + 5U*4U)
#define THR_EMPTY  0x20U

#define GenCounterFreq  (0x5F5E100 / 20)   // Generic Timer: 5MHz（= 1秒あたり5,000,000カウント）
#define TICK_CYCLES     (GenCounterFreq)  // 5,000,000カウント経過 = 1秒

#define GEN_TIMER_INTID  30
#define CNTP_CTL_EL0_ENABLE   (1U<<0)

#define STACKSIZE 0x1000
#define SEM_AVAILABLE TASKIDLE
#define NO_SEM NUMBER_OF_SEMS

extern void Schedule(void);
extern int switch_context(unsigned long *next_sp, unsigned long* sp);
extern void load_context(unsigned long *sp);
extern void ActivateInterrupt(uint32_t intID, uint32_t ipri, int type);
extern void SetupGIC(void);

extern uint32_t AckInterrupt(void);
extern uint32_t GetIRQId(uint32_t intid);
extern void EndOfInterrupt(uint32_t intid);
extern void DisableInt(void);
extern void EnableInt(void);
extern void default_vector_table(void);

typedef enum {
    TASK1 = 0,
    TASK2,
    TASK3,
    TASK4,
    TASK5,
    TASKIDLE,
    NUMBER_OF_TASKS,
} TaskIdType;

typedef enum {
    SEM1 = 0,
    SEM2,
    SEM3,
    SEM4,
    SEM5,
    NUMBER_OF_SEMS,
} SemIdType;

typedef struct {
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long x29;
    unsigned long x30;  // LR
} context;

struct TaskControl {
    enum { READY, BLOCKED} state;
    void (*entry)(void);
    long time_slice;
    long remaining_time;
    int expire;
    SemIdType target_sem;
    unsigned long sp;
    unsigned long task_stack[STACKSIZE] __attribute__((aligned(16)));
};

struct SemaphoreControl {
    TaskIdType owner_task;
};

void Task1(void);
void Task2(void);
void Task3(void);
void Task4(void);
void Task5(void);
void Idle(void);

void Snooze(int tim);

void AcquireSemaphore(SemIdType sem);
int  TryToAcquireSemaphore(SemIdType sem);
void ReleaseSemaphore(SemIdType sem);

void _TaskBlock(void);
void _TaskUnblock(TaskIdType task);

TaskIdType CurrentTask;

struct TaskControl TaskControl[NUMBER_OF_TASKS] = {
    {.entry = Task1, .state = READY, .time_slice = 2},
    {.entry = Task2, .state = READY, .time_slice = 4},
    {.entry = Task3, .state = READY, .time_slice = 1}, 
    {.entry = Task4, .state = READY, .time_slice = 3}, 
    {.entry = Task5, .state = READY, .time_slice = 1}, 
    {.entry = Idle,  .state = READY,  .time_slice = 1}, 
};

struct SemaphoreControl SemaphoreControl[NUMBER_OF_SEMS] = {
    {.owner_task = SEM_AVAILABLE},
    {.owner_task = SEM_AVAILABLE},
    {.owner_task = SEM_AVAILABLE},
    {.owner_task = SEM_AVAILABLE},
    {.owner_task = SEM_AVAILABLE}, 
};

static void clearbss(void)
{
    unsigned long long *p;
    extern unsigned long long _bss_start[];
    extern unsigned long long _bss_end[];

    for (p = _bss_start; p < _bss_end; p++) {
        *p = 0LL;
    }
}

void uart_putchar(char c){
    volatile uint32_t * const uart = (uint32_t *)UART_TX;
    volatile uint32_t * const status = (uint32_t *)UART_LSR;
    while ( !(*status & THR_EMPTY) ) ;
    *uart = c;
}

void _print_message_core ( const char* s, va_list ap ) {
    while (*s) {
        if (*s == '%' && *(s+1) == 'x') {
            unsigned long v = va_arg(ap, unsigned long);
            _Bool print_started = FALSE;
            int i;

            s += 2;
            for (i = 15; i >= 0; i--) {
                unsigned long x = (v & 0xFUL << i*4) >> i*4;
                if (print_started || x != 0U || i == 0) {
                    print_started = TRUE;
                    uart_putchar(x < 10 ? ('0' + x) : ('a' + x - 10));
                }
            }
        } else {
            if (*s == '\n') {
                uart_putchar('\r');
            }
            uart_putchar(*s++);
        }
    }
}

void _print_message(const char* s, ...){
    va_list ap;
    va_start ( ap, s );
    _print_message_core( s, ap );
    va_end ( ap );
}
void print_message(const char* s, ...){
    va_list ap;
    DisableInt();
    va_start ( ap, s );
    _print_message_core ( s, ap );
    va_end ( ap );
    EnableInt();
}

unsigned int myrandom(void)
{
    static unsigned long long x = 11;
    x = (48271 * x) % 2147483647;
    return (unsigned int)x;
}

static inline uint32_t GetIRQIdFromIAR(uint32_t iar) { return iar & 0x3FFu; }

int GetForks(SemIdType fork_left, SemIdType fork_right)
{
    AcquireSemaphore(fork_left);
    if (TryToAcquireSemaphore(fork_right)) {
        return TRUE;
    }
    ReleaseSemaphore(fork_left);
    return FALSE;
}

void ReleaseForks(SemIdType fork_left, SemIdType fork_right)
{
    ReleaseSemaphore(fork_left);
    ReleaseSemaphore(fork_right);
}

void PhilosopherMeditate()
{
    print_message("    Meditating\n");
    Snooze(myrandom() % 2 + 1);
}

void PhilosopherEat()
{
    print_message("    Eating\n");
    Snooze(myrandom() % 5 + 1);
}

void TaskJob(const TaskIdType task, const SemIdType fork_left, const SemIdType fork_right)
{
    char taskname[] = "Task?";
    taskname[4] = '1' + task;

    while (1) {
        while ( !GetForks(fork_left, fork_right) ) {
            print_message(taskname);
            PhilosopherMeditate();
        }
        print_message(taskname);
        PhilosopherEat();
        ReleaseForks(fork_left, fork_right);
    }
}

void Task1(void)
{
    TaskJob(TASK1, SEM1, SEM2);
}

void Task2(void)
{
    TaskJob(TASK2, SEM2, SEM3);
}

void Task3(void)
{
    TaskJob(TASK3, SEM3, SEM4);
}

void Task4(void)
{
    TaskJob(TASK4, SEM4, SEM5);
}

void Task5(void)
{
    TaskJob(TASK5, SEM5, SEM1);
}

void Idle(void)
{
    while (1) {
        asm volatile("wfi");
    }
}

void AcquireSemaphore(SemIdType sem)
{
    DisableInt();
    while (SemaphoreControl[sem].owner_task != SEM_AVAILABLE) {
        TaskControl[CurrentTask].target_sem = sem;
        _TaskBlock();
    }
    SemaphoreControl[sem].owner_task = CurrentTask;

    EnableInt();
}

int TryToAcquireSemaphore(SemIdType sem)
{
    DisableInt();
    if (SemaphoreControl[sem].owner_task == SEM_AVAILABLE) {
        SemaphoreControl[sem].owner_task = CurrentTask;
    }
    EnableInt();
    return SemaphoreControl[sem].owner_task == CurrentTask;
}

void ReleaseSemaphore(SemIdType sem)
{
    TaskIdType task;
    DisableInt();
    SemaphoreControl[sem].owner_task = SEM_AVAILABLE;
    for (task = 0; task < NUMBER_OF_TASKS; task++) {
        if (TaskControl[task].state == BLOCKED && TaskControl[task].target_sem == sem) {
            TaskControl[task].target_sem = NO_SEM;
            TaskControl[task].expire = 0;
            _TaskUnblock(task);
        }
    }
    EnableInt();
}

void SetVectorTable(void) {
    WriteSysReg(VBAR_EL1, (unsigned long)&default_vector_table);
    __asm__ volatile ("isb");
}

static TaskIdType ChooseNextTask(void)
{
    TaskIdType task = CurrentTask;

    do {
        task = (task + 1) % NUMBER_OF_TASKS;
    } while (TaskControl[task].state != READY && task != CurrentTask && task != TASKIDLE);

    if (TaskControl[task].state != READY) {
        task = TASKIDLE;
    }

    return task;
}

void TaskSwitch(struct TaskControl *current, struct TaskControl *next)
{
    switch_context(&next->sp, &current->sp);
}

void _Schedule(void)
{
    TaskIdType from = CurrentTask;
    CurrentTask = ChooseNextTask();
    if (from != CurrentTask) {
        TaskSwitch(&TaskControl[from], &TaskControl[CurrentTask]); 
    }
}

void Schedule(void)
{
    DisableInt();
    _Schedule();
    EnableInt();
}

void _TaskBlock(void)
{
    TaskControl[CurrentTask].state = BLOCKED;
    _Schedule();
}

void _TaskUnblock(TaskIdType task)
{
    TaskControl[task].state = READY;
    _Schedule();
}

void Snooze(int tim)
{
    DisableInt();
    TaskControl[CurrentTask].expire = tim;
    _TaskBlock();
    EnableInt();
}

static void TaskEntry(void)
{
    EnableInt();
    TaskControl[CurrentTask].entry();
}

static void InitTask(TaskIdType task)
{
    context* p = (context *)&TaskControl[task].task_stack[STACKSIZE] - 1;
    p->x30 = (unsigned long)TaskEntry;
    TaskControl[task].sp = (unsigned long)p;
    TaskControl[task].remaining_time = TaskControl[task].time_slice;
    TaskControl[task].target_sem = NO_SEM;
}

int Timer(void)
{
    TaskIdType task;
    _print_message("\nTimer\n\n");
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES);

    for (task = 0; task < NUMBER_OF_TASKS; task++) {
        if (TaskControl[task].state == BLOCKED && TaskControl[task].expire > 0) {
            if (--TaskControl[task].expire == 0) {
                TaskControl[task].state = READY;
            }
        }
    }

    if (--TaskControl[CurrentTask].remaining_time <= 0) {
        TaskControl[CurrentTask].remaining_time = TaskControl[CurrentTask].time_slice;
        return 1;
    }
    return 0;
}

int InterruptHandler(void)
{
    int preempt = 0;

    uint32_t iar = ReadGICC32(GICC_IAR);
    uint32_t irq = GetIRQIdFromIAR(iar);

    if (irq == GEN_TIMER_INTID) {
        preempt = Timer();
    }

    WriteGICC32(GICC_EOIR, iar);

    if (preempt) {
        _Schedule();
    }
    return 0;
}

static void StartTimer(void)
{
    ActivateInterrupt(GEN_TIMER_INTID, 16U, FALSE);
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES );
    WriteSysReg(CNTP_CTL_EL0, CNTP_CTL_EL0_ENABLE);
}

void main(void) {
    TaskIdType task;
    clearbss();
    SetVectorTable();
    SetupGIC();

    for (task = 0; task < NUMBER_OF_TASKS; task++) {
        InitTask(task);
    }

    StartTimer();

    CurrentTask = TASK1;
    load_context(&TaskControl[CurrentTask].sp);
}