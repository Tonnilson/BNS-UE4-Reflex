extern o_presentinject_1:QWORD
extern o_presentinject_2:QWORD
extern PresentLatencyMarkerStart:PROC
extern PresentLatencyMarkerEnd:PROC

.data

.code
PresentInject_1_ASM proc
call PresentLatencyMarkerStart
movzx r9d, byte ptr [rbx+36]
jmp [o_presentinject_1]
PresentInject_1_ASM endp

PresentInject_2_ASM proc
call PresentLatencyMarkerEnd
lea rcx,[rbp+296]
jmp [o_presentinject_2]
PresentInject_2_ASM endp
end