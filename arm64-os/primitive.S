
    .globl GetCoreID
    .type GetCoreID,@function
    .balign 4
GetCoreID:
    mrs x0, MPIDR_EL1
    ubfx    x0, x0, #0, #8              // coreid == affinity0
    ret
    .size GetCoreID,.-GetCoreID

