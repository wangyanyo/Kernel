#include "isr80h/isr80h.h"
#include "idt/idt.h"
#include "isr80h/misc.h"
#include "isr80h/io.h"

void isr80h_register_commands()
{
    isr80h_register_command(SYSTEM_COMMAND0_SUM, isr80h_command0_sum);
    isr80h_register_command(SYSTEM_COMMAND0_PRINT, isr80h_command_print);
}