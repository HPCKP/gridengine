/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "sge_signal.h"
#include "sge_string.h"

#include "msg_utilib.h"

#ifdef WIN32
#   define SIGIOT 6
#   define SIGURG 16
#   define SIGIO 23
#   define SIGVTALRM 26
#   define SIGPROF 27
#   define SIGWINCH 28
#endif

#if defined(CRAY) && !defined(SIGXCPU)
#   define SIGXCPU SIGCPULIM
#endif

#if defined(ALPHA)
#  undef NSIG
#  define NSIG (SIGUSR2+1)
#endif

sig_mapT sig_map[] = 
{
   {SGE_SIGHUP, SIGHUP, "HUP"},
   {SGE_SIGINT, SIGINT, "INT"},
   {SGE_SIGQUIT, SIGQUIT, "QUIT"},
   {SGE_SIGILL, SIGILL, "ILL"},
   {SGE_SIGTRAP, SIGTRAP, "TRAP"},
   {SGE_SIGABRT, SIGABRT, "ABRT"},
   {SGE_SIGIOT, SIGIOT, "IOT"},
#ifdef SIGEMT
   {SGE_SIGEMT, SIGEMT, "EMT"},
#else
   {SGE_SIGEMT, SIGUNKNOWN, "EMT"},
#endif
   {SGE_SIGFPE, SIGFPE, "FPE"},
   {SGE_SIGKILL, SIGKILL, "KILL"},
   {SGE_SIGSEGV, SIGSEGV, "SEGV"},
   {SGE_SIGPIPE, SIGPIPE, "PIPE"},
   {SGE_SIGALRM, SIGALRM, "ALRM"},
   {SGE_SIGTERM, SIGTERM, "TERM"},
   {SGE_SIGURG, SIGURG, "URG"},
   {SGE_SIGSTOP, SIGSTOP, "STOP"},
   {SGE_SIGTSTP, SIGTSTP, "TSTP"},
   {SGE_SIGCONT, SIGCONT, "CONT"},
   {SGE_SIGCHLD, SIGCHLD, "CHLD"},
   {SGE_SIGTTIN, SIGTTIN, "TTIN"},
   {SGE_SIGTTOU, SIGTTOU, "TTOU"},
   {SGE_SIGIO, SIGIO, "IO"},
#if !defined(HPUX)
   {SGE_SIGXCPU, SIGXCPU, "XCPU"},
#ifndef CRAY
   {SGE_SIGXFSZ, SIGXFSZ, "XFSZ"},
#endif
#endif
#if !(defined(CRAY) || defined(NECSX4) || defined(NECSX5))
   {SGE_SIGVTALRM, SIGVTALRM, "VTALRM"},
   {SGE_SIGPROF, SIGPROF, "PROF"},
#endif
   {SGE_SIGWINCH, SIGWINCH, "WINCH"},
   {SGE_SIGUSR1, SIGUSR1, "USR1"},
   {SGE_SIGUSR2, SIGUSR2, "USR2"},
   {SGE_SIGBUS, SIGBUS, "BUS"},
   {SGE_MIGRATE, SIGTTOU, "MIGRATE"},
   {0, 0}
};

/****** uti/signal/sge_unmap_signal() *****************************************
*  NAME
*     sge_unmap_signal() -- Unmap 32bit SGE/EE signal to system signal 
*
*  SYNOPSIS
*     int sge_unmap_signal(u_long32 sge_sig) 
*
*  FUNCTION
*     Unmap the 32bit SGE/EEsignal to the system specific signal 
*
*  INPUTS
*     u_long32 sge_sig - SGE/EE signal 
*
*  RESULT
*     int - system signal
******************************************************************************/
int sge_unmap_signal(u_long32 sge_sig) 
{
   sig_mapT *mapptr=sig_map;

   while (mapptr->sge_sig) {
      if (mapptr->sge_sig == sge_sig) {
         return mapptr->sig;
      }
      mapptr++;
   }
   return -1;
}

/****** uti/signal/sge_map_signal() *******************************************
*  NAME
*     sge_map_signal() -- Map system signal to 32bit SGE/EE signal 
*
*  SYNOPSIS
*     u_long32 sge_map_signal(int sys_sig) 
*
*  FUNCTION
*     Map the system specific signal to the 32bit sge signal 
*
*  INPUTS
*     int sys_sig - system signal 
*
*  RESULT
*     u_long32 - SGE/EE Signal
******************************************************************************/
u_long32 sge_map_signal(int sys_sig) 
{
   sig_mapT *mapptr=sig_map;

   while (mapptr->sge_sig) {
      if (mapptr->sig == sys_sig) {
         return mapptr->sge_sig;
      }
      mapptr++;
   }
   return -1;
}

/****** uti/signal/sge_str2signal() ********************************************
*  NAME
*     str2signal() -- Make a SGE/SGEEE signal out of a string 
*
*  SYNOPSIS
*     u_long32 sge_str2signal(const char *str) 
*
*  FUNCTION
*     Make a sge signal out of a string. 'str' can be the signal name 
*     (caseinsensitive) without sig or the signal number (Take care 
*     numbers are system dependent).
*
*  INPUTS
*     const char *str - signal string 
*
*  RESULT
*     u_long32 - SGE/EE signal 
******************************************************************************/
u_long32 sge_str2signal(const char *str) 
{
   sig_mapT *mapptr=sig_map;
   u_long32 signum;

   /* look for signal names in mapping table */
   while (mapptr->sge_sig) {
      if (!strcasecmp(str, mapptr->signame)) {
         return mapptr->sge_sig;
      }
      mapptr++;
   }

   /* could not find per name -> look for signal numbers */
   if (sge_strisint(str)) {
      signum = strtol(str, NULL, 10);
      mapptr = sig_map;
      while (mapptr->sge_sig) {
         if ((int) signum ==  mapptr->sig) {
            return mapptr->sge_sig;
         }
         mapptr++;
      }
   }

   return -1;
}

/****** uti/signal/sge_sys_str2signal() ***************************************
*  NAME
*     sge_sys_str2signal() -- Make a SGE/SGEEE signal out of a string 
*
*  SYNOPSIS
*     u_long32 sge_sys_str2signal(const char *str) 
*
*  FUNCTION
*     Make a SGE/SGEEE signal out of a string 
*
*  INPUTS
*     const char *str - signal name 
*
*  RESULT
*     u_long32 - SGE/EE signal
******************************************************************************/
u_long32 sge_sys_str2signal(const char *str) 
{
   sig_mapT *mapptr=sig_map;
   u_long32 signum;

   /* look for signal names in mapping table */
   while (mapptr->sge_sig) {
      if (!strcasecmp(str, mapptr->signame)) {
         return mapptr->sig;
      }
      mapptr++;
   }

   /* could not find per name -> look for signal numbers */
   if (sge_strisint(str)) {
      signum = strtol(str, NULL, 10);
      return signum;
   }

   return -1;
}

/****** uti/signal/sge_sig2str() **********************************************
*  NAME
*     sge_sig2str() -- Make a string out of a SGE/EE signal 
*
*  SYNOPSIS
*     const char* sge_sig2str(u_long32 sge_sig) 
*
*  FUNCTION
*     Make a string out of a SGE/EE signal    
*
*  INPUTS
*     u_long32 sge_sig - SGE/EE signal
*
*  RESULT
*     const char* - signal string
******************************************************************************/
const char *sge_sig2str(u_long32 sge_sig) 
{
   sig_mapT *mapptr;

   /* look for signal names in mapping table */
   for (mapptr=sig_map; mapptr->sge_sig; mapptr++) {
      if (sge_sig == mapptr->sge_sig) {
         return mapptr->signame;
      }
   }

   return MSG_PROC_UNKNOWNSIGNAL;
}

/****** uti/signal/sge_sys_sig2str() ******************************************
*  NAME
*     sge_sys_sig2str() -- Make a string out of a system signal 
*
*  SYNOPSIS
*     const char* sge_sys_sig2str(u_long32 sys_sig) 
*
*  FUNCTION
*     Make a string out of a system signal 
*
*  INPUTS
*     u_long32 sys_sig - system signal 
*
*  RESULT
*     const char* - signal string
******************************************************************************/
const char *sge_sys_sig2str(u_long32 sys_sig) 
{
   sig_mapT *mapptr;

   /* look for signal names in mapping table */
   for (mapptr=sig_map; mapptr->sge_sig; mapptr++) {
      if ((int) sys_sig == mapptr->sig) {
         return mapptr->signame;
      }
   }

   return MSG_PROC_UNKNOWNSIGNAL;
}

/****** uti/signal/sge_set_def_sig_mask() *************************************
*  NAME
*     sge_set_def_sig_mask() -- Set signal mask to default
*
*  SYNOPSIS
*     void sge_set_def_sig_mask(int sig_num, err_func_t err_func)
*
*  FUNCTION
*     Set signal mask to default for all signals except given signal
*
*  INPUTS
*     int sig_num         - signal number
*     err_func_t err_func - callback function to report errors
******************************************************************************/
void sge_set_def_sig_mask(int sig_num, err_func_t err_func)
{
   int i;
   struct sigaction sig_vec;
   sigset_t sigmask;
   char err_str[256];
 
   errno = 0;
   for (i=1; i < NSIG; i++) {
#if !defined(HP10) && !defined(HP10_01) && !defined(HPCONVEX) && !defined(HP11)
      if (i != SIGKILL && i != SIGSTOP && i != sig_num)
#else
      if (i != SIGKILL && i != SIGSTOP &&
          i != _SIGRESERVE && i != SIGDIL && i != sig_num)
#endif
      {
         sigemptyset(&sig_vec.sa_mask);
         sig_vec.sa_flags = 0;
         sig_vec.sa_handler = 0;
         sig_vec.sa_handler = SIG_DFL;
         if (sigaction(i, &sig_vec, NULL)) {
            sprintf(err_str, MSG_PROC_SIGACTIONFAILED_IS, i, strerror(errno));
            if (err_func) {
               err_func(err_str);
            }
         }
      }
   }
 
   /*
    * unblock all signals
    * without this we depend on shell to unblock the signals
    * result is that SIGXCPU was not delivered with several shells
    */
   sigemptyset(&sigmask);
   sigprocmask(SIG_SETMASK, &sigmask, NULL);
}   

