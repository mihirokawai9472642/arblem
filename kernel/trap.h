#pragma once

#define TRAP_FRAME_SIZE 144

#ifndef __ASSEMBLER__
void trap_enter_user(unsigned long pc, unsigned long sp);
#endif
