//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "sm-sbi.h"
#include "pmp.h"
#include "enclave.h"
#include "page.h"
#include "cpu.h"
#include <errno.h>
#include "platform.h"
#include "plugins/plugins.h"

uintptr_t mcall_sm_create_enclave(uintptr_t create_args)
{
  struct keystone_sbi_create create_args_local;
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = copy_enclave_create_args(create_args,
                       &create_args_local);

  if( ret != ENCLAVE_SUCCESS )
    return ret;

  ret = create_enclave(create_args_local);
  return ret;
}

uintptr_t mcall_sm_destroy_enclave(unsigned long eid)
{
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = destroy_enclave((unsigned int)eid);
  return ret;
}
uintptr_t mcall_sm_run_enclave(uintptr_t* regs, unsigned long eid)
{
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = run_enclave(regs, (unsigned int) eid);

  return ret;
}

uintptr_t mcall_sm_resume_enclave(uintptr_t* host_regs, unsigned long eid)
{
  enclave_ret_code ret;

  /* an enclave cannot call this SBI */
  if (cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = resume_enclave(host_regs, (unsigned int) eid);
  return ret;
}

uintptr_t mcall_sm_exit_enclave(uintptr_t* encl_regs, unsigned long retval)
{
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  /* calculate policy counter */
  int eid = cpu_get_enclave_id();
  enclave_policies[eid].instr_run_tot = enclave_policies[eid].instr_run_tot + ((uint64_t)read_csr(minstret) - enclave_policies[eid].instr_count);
  enclave_policies[eid].cycles_run_tot = enclave_policies[eid].cycles_run_tot + ((uint64_t)read_csr(mcycle) - enclave_policies[eid].cycle_count);
  printm("EID: %5d, %10s %ld, %10s %ld\n", eid, "instr_run_total:", enclave_policies[eid].instr_run_tot, "cycles_run_total:", enclave_policies[eid].cycles_run_tot);

  ret = exit_enclave(encl_regs, (unsigned long) retval, eid);
  return ret;
}

uintptr_t mcall_sm_stop_enclave(uintptr_t* encl_regs, unsigned long request)
{
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  /* calculate policy counter */
  int eid = cpu_get_enclave_id();
  enclave_policies[eid].instr_run_tot = enclave_policies[eid].instr_run_tot + ((uint64_t)read_csr(minstret) - enclave_policies[eid].instr_count);
  enclave_policies[eid].cycles_run_tot = enclave_policies[eid].cycles_run_tot + ((uint64_t)read_csr(mcycle) - enclave_policies[eid].cycle_count);
  printm("EID: %5d, %10s %ld, %10s %ld\n", eid, "instr_run_total:", enclave_policies[eid].instr_run_tot, "cycles_run_total:", enclave_policies[eid].cycles_run_tot);

  ret = stop_enclave(encl_regs, (uint64_t)request, eid);
  return ret;
}

uintptr_t mcall_sm_attest_enclave(uintptr_t report, uintptr_t data, uintptr_t size)
{
  enclave_ret_code ret;
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  ret = attest_enclave(report, data, size, cpu_get_enclave_id());
  return ret;
}

uintptr_t mcall_sm_get_sealing_key(uintptr_t sealing_key, uintptr_t key_ident,
                                   size_t key_ident_size)
{
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  return get_sealing_key(sealing_key, key_ident, key_ident_size,
                         cpu_get_enclave_id());
}

uintptr_t mcall_sm_random()
{
  /* Anyone may call this interface. */

  return platform_random();
}

uintptr_t mcall_sm_call_plugin(uintptr_t plugin_id, uintptr_t call_id, uintptr_t arg0, uintptr_t arg1)
{
  if(!cpu_is_enclave_context()) {
    return ENCLAVE_SBI_PROHIBITED;
  }

  return call_plugin(cpu_get_enclave_id(), plugin_id, call_id, arg0, arg1);
}

/* TODO: this should be removed in the future. */
uintptr_t mcall_sm_not_implemented(uintptr_t* encl_regs, unsigned long cause)
{
  /* only an enclave itself can call this SBI */
  if (!cpu_is_enclave_context()) {
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

  return exit_enclave(encl_regs, (uint64_t)-1UL, cpu_get_enclave_id());
}
