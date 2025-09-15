; Simple 8080 test program
; Adds two numbers and stores the result.

        org     100h        ; Start at address 0x100

start:  mvi     a, 5        ; Load the first number (5) into the accumulator
        mvi     b, 10       ; Load the second number (10) into register B
        add     b           ; Add register B to the accumulator (A = A + B)
        sta     result      ; Store the accumulator's value at the 'result' address
        hlt                 ; Halt the processor

; Data storage area
result: ds      1           ; Reserve 1 byte of storage for the result

        end                 ; End of the assembly program