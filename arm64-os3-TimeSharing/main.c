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

#define GenCounterFreq  (0x5F5E100/20)   //5M
#define TICK_CYCLES     (GenCounterFreq/1)
#define GEN_TIMER_INTID  30
#define CNTP_CTL_EL0_ENABLE   (1U<<0)

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

void Task1(void);
void Task2(void);
void Task3(void);
static inline void maybe_resched(void);

void uart_putchar(char c){
    volatile uint32_t * const uart = (uint32_t *)UART_TX;
    volatile uint32_t * const status = (uint32_t *)UART_LSR;
    while ( !(*status & THR_EMPTY) ) ;
    *uart = c;
}

void print_message(const char* s, ...){
    DisableInt();
    va_list ap;
    va_start (ap, s);

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

    va_end (ap);
    EnableInt();
}

void timer_print_message(const char* s, ...){
    va_list ap;
    va_start (ap, s);

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

    va_end (ap);
}

typedef enum {
    TASK1 = 0,
    TASK2,
    TASK3,
    NUMBER_OF_TASKS,
} TaskIdType;

#define STACKSIZE 0x1000

typedef struct {
    unsigned long lr;   //Link Register 30
    unsigned long x17;
    unsigned long x29;
    unsigned long x18;
    unsigned long x27;
    unsigned long x28;
    unsigned long x25;
    unsigned long x26;
    unsigned long x23;
    unsigned long x24;
    unsigned long x21;
    unsigned long x22;
    unsigned long x19;
    unsigned long x20;
} context;

struct TaskControl {
    void (*entry)(void);
    unsigned long sp;
    long time_slice;
    long remaining_time;
    unsigned long task_stack[STACKSIZE] __attribute__((aligned(16)));
} TaskControl[NUMBER_OF_TASKS] = {
    {.entry = Task1, .time_slice = 2},
    {.entry = Task2, .time_slice = 4},
    {.entry = Task3, .time_slice = 1},
};

extern void TaskSwitch(struct TaskControl *current, struct TaskControl *next);

TaskIdType CurrentTask;

void SetVectorTable(void) {
    WriteSysReg(VBAR_EL1, (unsigned long)&default_vector_table);
    __asm__ volatile ("isb");
}

static void put_char(char c)
{
    volatile unsigned char * const uart = (unsigned char *)0x10000000U;
    *uart = c;
}

void Task1(void)
{
    while (1) {
        print_message("Task1\n");
        maybe_resched();
    }
}

void Task2(void)
{
    while (1) {
        print_message("Task2\n");
        maybe_resched();
    }
}

void Task3(void)
{
    while (1) {
        print_message("Task3\n");
        Schedule();
    }
}

void TaskSwitch(struct TaskControl *current, struct TaskControl *next)
{
    switch_context(&next->sp, &current->sp);
}

static TaskIdType ChooseNextTask(void)
{
    return (CurrentTask + 1) % NUMBER_OF_TASKS;
}

void _Schedule(void)
{
    TaskIdType from = CurrentTask;
    CurrentTask = ChooseNextTask();
    TaskSwitch(&TaskControl[from], &TaskControl[CurrentTask]);
}

void Schedule(void)
{
    DisableInt();
    _Schedule();
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
    p->lr = (unsigned long)TaskEntry;
    TaskControl[task].sp = (unsigned long)p;
    TaskControl[task].remaining_time = TaskControl[task].time_slice;
}

static void clearbss(void)
{
    unsigned long long *p;
    extern unsigned long long _bss_start[];
    extern unsigned long long _bss_end[];

    for (p = _bss_start; p < _bss_end; p++) {
        *p = 0LL;
    }
}


int Timer(void)
{
    timer_print_message("\nTimer\n\n");
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES);
    if (--TaskControl[CurrentTask].remaining_time <= 0) {
        TaskControl[CurrentTask].remaining_time = TaskControl[CurrentTask].time_slice;
        return 1;
    }
    return 0;
}

static inline uint32_t GetIRQIdFromIAR(uint32_t iar) { return iar & 0x3FFu; }

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

static inline void maybe_resched(void)
{
    for (volatile int i = 0; i < 100000; i++) { __asm__ volatile("nop"); }
}

static void StartTimer(void)
{
    ActivateInterrupt(GEN_TIMER_INTID, 16U, FALSE);
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES );
    WriteSysReg(CNTP_CTL_EL0, CNTP_CTL_EL0_ENABLE);
}

void main(void) {
    clearbss();
    SetVectorTable();
    SetupGIC();
    StartTimer();

    InitTask(TASK1);
    InitTask(TASK2);
    InitTask(TASK3);

    CurrentTask = TASK1;
    load_context(&TaskControl[CurrentTask].sp);
}