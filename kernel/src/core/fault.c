#include <stdint.h>

#include "minemu/trace.h"
#include "minemu/trap.h"

/* There is no process to kill yet, so a fault from either mode records a
 * trace event (low byte = exception id) and stops the machine. */

#define TRACE_FAULT_BASE UINT32_C(0xfa170000)

static void fault_stop(const struct minemu_trap_frame *frame) {
    minemu_trace_event(TRACE_FAULT_BASE | ((uint32_t)frame->exception_id & 0xffU));
    minemu_fail_stop();
}

void minemu_undefined_dispatch(struct minemu_trap_frame *frame) {
    fault_stop(frame);
}

void minemu_abort_dispatch(struct minemu_trap_frame *frame) {
    fault_stop(frame);
}
