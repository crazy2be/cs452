.text
.globl context_switch
context_switch:
    stmfd r13, {r0-r9}
    ldmfd r13, {r0-r9}
    bx r14
