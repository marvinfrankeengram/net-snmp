#include <config.h>

/* Ripped from /usr/scr/usr.bin/vmstat/vmstat.c (covering all bases) */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>


#include "mibincl.h"
#include "util_funcs.h"

#include "vmstat.h"

/* nlist symbols */
#define CPTIME_SYMBOL   "cp_time"
#define SUM_SYMBOL      "cnt"
#define INTRCNT_SYMBOL  "intrcnt"
#define EINTRCNT_SYMBOL "eintrcnt"
#define BOOTTIME_SYMBOL "boottime"

/* Number of interrupts */
#define INT_COUNT       10

/* CPU percentage */
#define CPU_PRC         100

void init_vmstat_freebsd2(void) 
{

  struct variable2 extensible_vmstat_variables[] = {
    {MIBINDEX, ASN_INTEGER, RONLY, var_extensible_vmstat,1,{MIBINDEX}},
    {ERRORNAME, ASN_OCTET_STR, RONLY, var_extensible_vmstat, 1, {ERRORNAME }},
    {SWAPIN, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {SWAPIN}},
    {SWAPOUT, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {SWAPOUT}},
    {IOSENT, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {IOSENT}},
    {IORECEIVE, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {IORECEIVE}},
    {SYSINTERRUPTS, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {SYSINTERRUPTS}},
    {SYSCONTEXT, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {SYSCONTEXT}},
    {CPUUSER, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {CPUUSER}},
    {CPUSYSTEM, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {CPUSYSTEM}},
    {CPUIDLE, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {CPUIDLE}},
/* Future use: */
/*
  {ERRORFLAG, ASN_INTEGER, RONLY, var_extensible_vmstat, 1, {ERRORFLAG }},
  {ERRORMSG, ASN_OCTET_STR, RONLY, var_extensible_vmstat, 1, {ERRORMSG }}
*/
  };

  /* Define the OID pointer to the top of the mib tree that we're
   registering underneath */
  oid vmstat_variables_oid[] = { EXTENSIBLEMIB,11 };

  /* register ourselves with the agent to handle our mib tree */
  REGISTER_MIB("ucd-snmp/vmstat", extensibel_vmstat_variables, variable2, \
               vmstat_variables_oid);
  
}


long
getuptime(void )
{
	static time_t now, boottime;
	time_t uptime;

	if (boottime == 0)
		auto_nlist(BOOTTIME_SYMBOL, &boottime, sizeof (boottime));

	time(&now);
	uptime = now - boottime;

	return(uptime);
}

unsigned char *var_extensible_vmstat(struct variable *vp,
				     oid *name,
				     int *length,
				     int exact,
				     int *var_len,
				     WriteMethod **write_method)
{

    int loop;

    time_t time_new = getuptime();
    static time_t time_old;
    static time_t time_diff;

    static long cpu_old[CPUSTATES];
    static long cpu_new[CPUSTATES];
    static long cpu_diff[CPUSTATES];
    static long cpu_total;
    long cpu_sum;
    double cpu_prc;

    static struct vmmeter mem_old, mem_new;

    static long long_ret;
    static char errmsg[300];

    long_ret = 0;  /* set to 0 as default */

    if (!checkmib(vp,name,length,exact,var_len,write_method,1))
	return(NULL);

    /* Update structures (only if time has passed) */
    if (time_new != time_old)
    {
	time_diff = time_new - time_old;
	time_old = time_new;

	/* CPU usage */
	auto_nlist(CPTIME_SYMBOL, (char *)cpu_new, sizeof (cpu_new));
	
	cpu_total = 0;
	
	for (loop = 0; loop < CPUSTATES; loop++)
	{
	    cpu_diff[loop] = cpu_new[loop] - cpu_old[loop];
	    cpu_old[loop] = cpu_new[loop];
	    cpu_total += cpu_diff[loop];
	}
	
	if (cpu_total == 0) cpu_total = 1;

	/* Memory info */
	mem_old = mem_new;
	auto_nlist(SUM_SYMBOL, &mem_new, sizeof(mem_new));
    }

/* Rate macro */
#define rate(x) (((x)+ time_diff/2) / time_diff)

/* Page-to-kb macro */
#define ptok(p) ((p) * (mem_new.v_page_size >> 10))

    switch (vp->magic) {
    case MIBINDEX:
	long_ret = 1;
	return((u_char *) (&long_ret));
    case ERRORNAME:    /* dummy name */
	sprintf(errmsg,"systemStats");
	*var_len = strlen(errmsg);
	return((u_char *) (errmsg));
    case SWAPIN:
	long_ret = ptok(mem_new.v_swapin - mem_old.v_swapin + 
			mem_new.v_vnodein - mem_old.v_vnodein);
	long_ret = rate(long_ret);
	return((u_char *) (&long_ret));
    case SWAPOUT:
	long_ret = ptok(mem_new.v_swapout - mem_old.v_swapout + 
			mem_new.v_vnodeout - mem_old.v_vnodeout);
	long_ret = rate(long_ret);
	return((u_char *) (&long_ret));
    case IOSENT:
	long_ret = -1;
	return((u_char *) (&long_ret));
    case IORECEIVE:
	long_ret = -1;
	return((u_char *) (&long_ret));
    case SYSINTERRUPTS:
	long_ret = rate(mem_new.v_intr - mem_old.v_intr);
	return((u_char *) (&long_ret));
    case SYSCONTEXT:
	long_ret = rate(mem_new.v_swtch - mem_old.v_swtch);
	return((u_char *) (&long_ret));
    case CPUUSER:
	cpu_sum = cpu_diff[CP_USER] + cpu_diff[CP_NICE];
	cpu_prc = (float)cpu_sum / (float)cpu_total;
	long_ret = cpu_prc * CPU_PRC;
	return((u_char *) (&long_ret));
    case CPUSYSTEM:
	cpu_sum = cpu_diff[CP_SYS] + cpu_diff[CP_INTR];
	cpu_prc = (float)cpu_sum / (float)cpu_total;
	long_ret = cpu_prc * CPU_PRC;
	return((u_char *) (&long_ret));
    case CPUIDLE:
	cpu_sum = cpu_diff[CP_IDLE];
	cpu_prc = (float)cpu_sum / (float)cpu_total;
	long_ret = cpu_prc * CPU_PRC;
	return((u_char *) (&long_ret));
/* reserved for future use */
/*
  case ERRORFLAG:
  return((u_char *) (&long_ret));
  case ERRORMSG:
  return((u_char *) (&long_ret));
  */
    }
    return NULL;
}

