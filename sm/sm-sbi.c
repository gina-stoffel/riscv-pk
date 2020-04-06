//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "atomic.h"
#include "sm-sbi.h"
#include "pmp.h"
#include "enclave.h"
#include "page.h"
#include "cpu.h"
#include <errno.h>
#include "platform.h"
#include "plugins/plugins.h"

static spinlock_t sbi_lock = SPINLOCK_INIT;

void lock_sbi() {
  while (spinlock_trylock(&sbi_lock)) {
    pmp_ipi_update();
  }
}

void unlock_sbi() {
  spinlock_unlock(&sbi_lock);
}

uintptr_t mcall_sm_create_enclave(uintptr_t create_args)
{
  lock_sbi();
  struct keystone_sbi_create create_args_local;
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = copy_from_host((struct keystone_sbi_create*)create_args,
                       &create_args_local,
                       sizeof(struct keystone_sbi_create));

  if( ret != ENCLAVE_SUCCESS ) {
    unlock_sbi();
    return ret;
  }

  ret = create_enclave(create_args_local);
  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_destroy_enclave(unsigned long eid)
{
  lock_sbi();
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = destroy_enclave((unsigned int)eid);
  unlock_sbi();
  return ret;
}
uintptr_t mcall_sm_run_enclave(uintptr_t* regs, unsigned long eid)
{
  lock_sbi();
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = run_enclave(regs, (unsigned int) eid);
  unlock_sbi();

  return ret;
}

uintptr_t mcall_sm_resume_enclave(uintptr_t* host_regs, unsigned long eid)
{
  lock_sbi();
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = resume_enclave(host_regs, (unsigned int) eid);
  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_exit_enclave(uintptr_t* encl_regs, unsigned long retval)
{
  lock_sbi();
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = exit_enclave(encl_regs, (unsigned long) retval, cpu_get_enclave_id());
  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_stop_enclave(uintptr_t* encl_regs, unsigned long request)
{
  lock_sbi();
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = stop_enclave(encl_regs, (uint64_t)request, cpu_get_enclave_id());
  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_attest_enclave(uintptr_t report, uintptr_t data, uintptr_t size)
{
  lock_sbi();
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = attest_enclave(report, data, size, cpu_get_enclave_id());
  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_random()
{
  lock_sbi();
  /* Anyone may call this interface. */

  enclave_ret_code ret = platform_random();

  unlock_sbi();
  return ret;
}

uintptr_t mcall_sm_call_plugin(uintptr_t plugin_id, uintptr_t call_id, uintptr_t arg0, uintptr_t arg1)
{
  lock_sbi();
  if(!cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }
  enclave_ret_code ret = call_plugin(cpu_get_enclave_id(), plugin_id, call_id, arg0, arg1);
  unlock_sbi();
  return ret;
}

/* TODO: this should be removed in the future. */
uintptr_t mcall_sm_not_implemented(uintptr_t* encl_regs, unsigned long cause)
{
  lock_sbi();
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    unlock_sbi();
    return ENCLAVE_SBI_PROHIBITED;
  }

  if((long)cause < 0)
  {
    // discard MSB
    cause = cause << 1;
    cause = cause >> 1;
    printm("the runtime could not handle interrupt %ld\r\n", cause );
    printm("mideleg: 0x%lx\r\n");

  }
  else
  {
    printm("the runtime could not handle exception %ld\r\n", cause);
    printm("medeleg: 0x%lx (expected? %ld)\r\n", read_csr(medeleg), read_csr(medeleg) & (1<<cause));
  }
  enclave_ret_code ret = exit_enclave(encl_regs, (uint64_t)-1UL, cpu_get_enclave_id());
  unlock_sbi();
  return ret;
}
