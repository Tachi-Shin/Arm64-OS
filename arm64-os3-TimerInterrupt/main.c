#include <stdarg.h>
#include <stdint.h>
#include "armv8util.h"
#include "gicv2.h"

#define TRUE  1
#define FALSE 0

#define UART_BASE 0xFE215040U
#define UART_TX   (UART_BASE + 0U*4U)
#define UART_LSR  (UART_BASE + 5U*4U)
#define THR_EMPTY 0x20U

#define GenCounterFreq  (0x5F5E100 / 20)   // Generic Timer: 5MHz（= 1秒あたり5,000,000カウント）
#define TICK_CYCLES     (GenCounterFreq)  // 5,000,000カウント経過 = 1秒

#define GEN_TIMER_INTID  30
#define CNTP_CTL_EL0_ENABLE   (1U<<0)

#define STACKSIZE 0x1000

typedef enum {
    TASK1 = 0,
    TASK2,
    TASK3,
    NUMBER_OF_TASKS,
} TaskIdType;

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
    unsigned long sp;
    unsigned long task_stack[STACKSIZE] __attribute__((aligned(16)));
} TaskControl[NUMBER_OF_TASKS];

extern void Schedule(void);
extern int  switch_context(unsigned long *next_sp, unsigned long* sp);
extern void load_context(unsigned long *sp);
extern void TaskSwitch(struct TaskControl *current, struct TaskControl *next);
extern void ActivateInterrupt(uint32_t intID, uint32_t ipri, int type);
extern void SetupGIC(void);
extern void EnableInt(void);
extern void DisableInt(void);

static void clearbss(void);
void uart_putchar(char c);
void print_message(const char* s, ...);

void Task1(void);
void Task2(void);
void Task3(void);

static TaskIdType ChooseNextTask(void);
void Schedule(void);
void TaskSwitch(struct TaskControl *current, struct TaskControl *next);
void InitTask(TaskIdType task, void (*entry)());
extern void default_vector_table(void);

static void put_char(char c);
TaskIdType CurrentTask;

static void clearbss(void)
{
    unsigned long long *p;
    extern unsigned long long _bss_start[];
    extern unsigned long long _bss_end[];

    for (p = _bss_start; p < _bss_end; p++) {
        *p = 0LL;
    }
}

void uart_putchar(char c)
{
    volatile uint32_t * const uart   = (uint32_t *)UART_TX;
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

void Task1(void)
{
    while (1) {
        print_message("Task1\n");
        Schedule();
    }
}

void Task2(void)
{
    while (1) {
        print_message("Task2\n");
        Schedule();
    }
}

void Task3(void)
{
    while (1) {
        print_message("Task3\n");
        Schedule();
    }
}

void SetVectorTable(void) {
    WriteSysReg(VBAR_EL1, (unsigned long)&default_vector_table);
    __asm__ volatile ("isb");
}

static TaskIdType ChooseNextTask(void)
{
    return (CurrentTask + 1) % NUMBER_OF_TASKS;
}

void TaskSwitch(struct TaskControl *current, struct TaskControl *next)
{
    switch_context(&next->sp, &current->sp);
}

void Schedule(void)
{
    TaskIdType from = CurrentTask;
    CurrentTask = ChooseNextTask();
    TaskSwitch(&TaskControl[from], &TaskControl[CurrentTask]);
}

void InitTask(TaskIdType task, void (*entry)())
{
    context* p = (context *)&TaskControl[task].task_stack[STACKSIZE] - 1;
    p->x30 = (unsigned long)entry;
    TaskControl[task].sp = (unsigned long)p;
}

void Timer(void)
{
    print_message("\nTimer\n\n");
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES);
}

static inline uint32_t GetIRQIdFromIAR(uint32_t iar) { return iar & 0x3FFu; }

void InterruptHandler(void)
{
    uint32_t iar = ReadGICC32(GICC_IAR);
    uint32_t irq = GetIRQIdFromIAR(iar);

    if (irq == GEN_TIMER_INTID) {
        Timer();
    }

    WriteGICC32(GICC_EOIR, iar);
}

static void StartTimer(void)
{
    ActivateInterrupt(GEN_TIMER_INTID, 16U, FALSE);
    WriteSysReg(CNTP_CVAL_EL0, ReadSysReg(CNTPCT_EL0) + TICK_CYCLES );
    WriteSysReg(CNTP_CTL_EL0, CNTP_CTL_EL0_ENABLE);
}

void main(void)
{
    clearbss();
    SetVectorTable();
    SetupGIC();
    StartTimer();

    InitTask(TASK1, Task1);
    InitTask(TASK2, Task2);
    InitTask(TASK3, Task3);

    CurrentTask = TASK1;
    EnableInt();
    load_context(&TaskControl[CurrentTask].sp);
}