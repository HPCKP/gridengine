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
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <strings.h>

#ifndef WIN32NATIVE
#	include <unistd.h>
#endif

#include "sge_gdi_intern.h"
#include "commlib.h"
#include "sge_prognames.h"
#include "sgermon.h"
#include "sge_answerL.h"
#include "sge_eventL.h"
#include "sge_reportL.h"
#include "sge_c_event.h"
#include "utility.h"
#include "qm_name.h"
#include "sge_log.h"
#include "sge_time.h"
#include "msg_schedd.h"
#include "sge_exit.h"


static u_long32 ck_event_number(lList *lp, u_long32 *waiting_for, u_long32 *wrong_number);
static int get_event_list(int sync, lList **lp);
static void ec_config_changed(void);

static int config_changed = 0;

/*---------------------------------------- 
 *  this flag is used to control 
 *  registering and reregistering 
 *  at qmaster in case of error 
 *----------------------------------------*/
static int need_register = 1;

static lListElem *ec;
static u_long32 ec_reg_id = 0;

int ec_prepare_registration(u_long32 id, const char *name)
{
   char subscription[sgeE_EVENTSIZE + 1];
   int i;

   DENTER(TOP_LAYER, "ec_prepare_registration");

   if(id >= EV_ID_FIRST_DYNAMIC || name == NULL || *name == 0) {
      WARNING((SGE_EVENT, MSG_EVENT_ILLEGAL_ID_OR_NAME_US, u32c(id), name != NULL ? name : "NULL" ));
      DEXIT;
      return 0;
   }

   ec = lCreateElem(EV_Type);

   if(ec == NULL) {
      DEXIT; /* error message already comes from lCreateElem */
      return 0;
   }

   /* remember registration id for subsequent registrations */
   ec_reg_id = id;

   /* initialize event client object */
   lSetString(ec, EV_name, name);
   lSetUlong(ec, EV_d_time, DEFAULT_EVENT_DELIVERY_INTERVAL);

   /* initialize subscription "bitfield" */
   for(i = 0; i < sgeE_EVENTSIZE; i++) {
      subscription[i] = '0';
   }

   subscription[sgeE_EVENTSIZE] = 0;

   lSetString(ec, EV_subscription, subscription);
   ec_subscribe(sgeE_QMASTER_GOES_DOWN);
   ec_subscribe(sgeE_SHUTDOWN);

   DEXIT;
   return 1;
}

/*-------------------------------------------------------------*/
void ec_mark4registration(void)
{
   extern int new_global_config;
   DENTER(TOP_LAYER, "ec_mark4registration");
   need_register = 1;
   new_global_config = 1;
   DEXIT;
}

int ec_need_new_registration(void)
{
   return need_register;
}

/* return 1 if parameter has changed 
          0 otherwise                */
int ec_set_edtime(int interval) {
   int ret;

   DENTER(TOP_LAYER, "ec_set_edtime");
   
   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   ret = (lGetUlong(ec, EV_d_time) != interval);

   if(ret) {
      lSetUlong(ec, EV_d_time, interval);
      ec_config_changed();
   }

   DEXIT;
   return ret;
}

int ec_get_edtime(void) {
   int interval;

   DENTER(TOP_LAYER, "ec_get_edtime");

   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   interval = lGetUlong(ec, EV_d_time);
   DEXIT;
   return interval;
}

/*---------------------------------------------------------
 * ec_register
 *
 *  this has also be done in case the
 *  qmaster was down and the scheduler
 *  reenrolls at qmaster 
 *  return 1 if success  
 *         0 in case of failure
 *---------------------------------------------------------*/
int ec_register(void)
{
   int success = 0;
   lList *lp, *alp;
   lListElem *aep;

   DENTER(TOP_LAYER, "ec_register");

   if(ec_reg_id >= EV_ID_FIRST_DYNAMIC || ec == NULL) {
      WARNING((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      return 0;
   }

   /*
    *   EV_host, EV_commproc and EV_commid get filled
    *  at qmaster side with more secure commd    
    *  informations 
    *
    *  EV_uid gets filled with gdi_request
    *  informations
    */

   lSetUlong(ec, EV_id, ec_reg_id);

   /* initialize, we could do a re-registration */
   lSetUlong(ec, EV_last_heard_from, 0);
   lSetUlong(ec, EV_last_send_time, 0);
   lSetUlong(ec, EV_next_send_time, 0);
   lSetUlong(ec, EV_next_number, 0);


   lp = lCreateList("registration", EV_Type);
   lAppendElem(lp, lCopyElem(ec));

   /* remove possibly pending messages */
   remove_pending_messages(NULL, 0, 0, TAG_REPORT_REQUEST);
   /* commlib call to mark all commprocs as unknown */
   reset_last_heard();


   /*
    *  to add may also means to modify
    *  - if this event client is already enrolled at qmaster
    */
   alp = sge_gdi(SGE_EVENT_LIST, SGE_GDI_ADD | SGE_GDI_RETURN_NEW_VERSION, &lp, NULL, NULL);
 
   aep = lFirst(alp);
 
   success = (lGetUlong(aep, AN_status)==STATUS_OK);   

   if (success) { 
      lListElem *new_ec;
      u_long32 new_id = 0;

      new_ec = lFirst(lp);
      if(new_ec != NULL) {
         new_id = lGetUlong(new_ec, EV_id);
      }

      if(new_id != 0) {
         lSetUlong(ec, EV_id, new_id);
         DPRINTF(("REGISTERED with id "U32CFormat"\n", new_id));
         lFreeList(lp); 
         lFreeList(alp);

         config_changed = 0;
         
         DEXIT;
         return 1;
      }
   } else {
      if (lGetUlong(aep, AN_quality) == NUM_AN_ERROR) {
         ERROR((SGE_EVENT, "%s", lGetString(aep, AN_text)));
         lFreeList(lp);
         lFreeList(alp);
         DEXIT;
         SGE_EXIT(1);
         return 0;
      }   
   }

   lFreeList(lp); 
   lFreeList(alp);
   DPRINTF(("REGISTRATION FAILED\n"));
   DEXIT;
   return 0;
}      

int ec_deregister(void)
{
   int ret;
   sge_pack_buffer pb;

   DENTER(TOP_LAYER, "ec_deregister");

   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   if(init_packbuffer(&pb, sizeof(u_long32), 0) != PACK_SUCCESS) {
      /* error message is output from init_packbuffer */
      DEXIT;
      return 0;
   }

   packint(&pb, lGetUlong(ec, EV_id));

   ret = sge_send_any_request(0, NULL, sge_get_master(0), 
         prognames[QMASTER], 1, &pb, TAG_EVENT_CLIENT_EXIT);
   
   clear_packbuffer(&pb);

   if(ret != CL_OK) {
      /* error message is output from sge_send_any_request */
      DEXIT;
      return 0;
   }

   DEXIT;
   return 1;
}

int ec_subscribe(int event)
{
   char *subscription;
   
   DENTER(TOP_LAYER, "ec_subscribe");

   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   if(event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING((SGE_EVENT, MSG_EVENT_ILLEGALEVENTID_I, event));
      DEXIT;
      return 0;
   }

   subscription = strdup(lGetString(ec, EV_subscription));

   if(subscription == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   if(event == sgeE_ALL_EVENTS) {
      int i;
      for(i = 0; i < sgeE_EVENTSIZE; i++) {
         subscription[i] = '1';
      }
   } else {
      subscription[event] = '1';
   }

   if(strcmp(subscription, lGetString(ec, EV_subscription)) != 0) {
      lSetString(ec, EV_subscription, subscription);
      ec_config_changed();
   }

   free(subscription);

   DEXIT;
   return 1;
}

int ec_subscribe_all(void)
{
   return ec_subscribe(sgeE_ALL_EVENTS);
}

int ec_unsubscribe(int event)
{
   char *subscription;
   
   DENTER(TOP_LAYER, "ec_unsubscribe");

   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   if(event < sgeE_ALL_EVENTS || event >= sgeE_EVENTSIZE) {
      WARNING((SGE_EVENT, MSG_EVENT_ILLEGALEVENTID_I, event ));
      DEXIT;
      return 0;
   }

   subscription = strdup(lGetString(ec, EV_subscription));

   if(subscription == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   if(event == sgeE_ALL_EVENTS) {
      int i;
      for(i = 0; i < sgeE_EVENTSIZE; i++) {
         subscription[i] = '0';
      }
      subscription[sgeE_QMASTER_GOES_DOWN] = '1';
      subscription[sgeE_SHUTDOWN] = '1';
   } else {
      if(event == sgeE_QMASTER_GOES_DOWN || event == sgeE_SHUTDOWN) {
         ERROR((SGE_EVENT, MSG_EVENT_HAVETOHANDLEEVENTS));
         free(subscription);
         DEXIT;
         return 0;
      } else {
         subscription[event] = '0';
      }
   }

   if(strcmp(subscription, lGetString(ec, EV_subscription)) != 0) {
      lSetString(ec, EV_subscription, subscription);
      ec_config_changed();
   }
   
   free(subscription);

   DEXIT;
   return 1;
}

int ec_unsubscribe_all(void)
{
   return ec_unsubscribe(sgeE_ALL_EVENTS);
}

void ec_config_changed(void) 
{
   config_changed = 1;
}

int ec_commit(void) 
{
   int success;
   lList *lp, *alp;

   DENTER(TOP_LAYER, "ec_commit");

   /* not yet initialized? Cannot send modification to qmaster! */
   if(ec_reg_id >= EV_ID_FIRST_DYNAMIC || ec == NULL) {
      DPRINTF((MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return 0;
   }

   /* not (yet) registered? Cannot send modification to qmaster! */
   if(ec_need_new_registration()) {
      DPRINTF((MSG_EVENT_NOTREGISTERED));
      DEXIT;
      return 0;
   }

   if(!config_changed) {
      DPRINTF(("no changes to commit\n"));
      DEXIT;
      return 0;
   }

   lp = lCreateList("change configuration", EV_Type);
   lAppendElem(lp, lCopyElem(ec));

   /*
    *  to add may also means to modify
    *  - if this event client is already enrolled at qmaster
    */
   alp = sge_gdi(SGE_EVENT_LIST, SGE_GDI_MOD, &lp, NULL, NULL);
   lFreeList(lp); 
   
   success = (lGetUlong(lFirst(alp), AN_status)==STATUS_OK);   
   lFreeList(alp);
   
   if (success) {
      config_changed = 0;
      DPRINTF(("CHANGE CONFIGURATION SUCCEEDED\n"));
      DEXIT;
      return 1;
   }

   DPRINTF(("CHANGE CONFIGURATION FAILED\n"));
   DEXIT;
   return 0;
}

/*-------------------------------------------------------------------*
 * ec_get
 * 
 * returns
 *    0 ok got events or got empty event list or got no message
 *   <0 commlib error
 *-------------------------------------------------------------------*/
int ec_get(
lList **event_list
) {  
   int ret;
   lList *report_list = NULL;
   static u_long32 next_event = 1;
   u_long32 wrong_number;
   int sync;
   
   DENTER(TOP_LAYER, "ec_get");
 
   if(ec == NULL) {
      ERROR((SGE_EVENT, MSG_EVENT_UNINITIALIZED_EC));
      DEXIT;
      return -1;
   }
 
   if (ec_need_new_registration()) {
      next_event = 1;
      if (!ec_register()) {
         DEXIT;
         return -1;
      }
      else
         need_register = 0;
   }
      
   if(config_changed) {
      ec_commit();
   }
      
   /* receive_message blocks in the first loop */
   for (sync = 1; !(ret = get_event_list(sync, &report_list)); sync = 0) {
      
      lList *new_events = NULL;
      lXchgList(lFirst(report_list), REP_list, &new_events);
      report_list = lFreeList(report_list);

      if (ck_event_number(new_events, &next_event, &wrong_number)) {
         /*
          *  may be got an old event, that was sent before
          *  reregistration at qmaster
          */
         lFreeList(*event_list);
         lFreeList(new_events);
         ec_mark4registration();
         DEXIT;
         return -1;
      }

      DPRINTF(("got events till "u32"\n", next_event-1));

      /* append new_events to event_list */
      if (*event_list) 
         lAddList(*event_list, new_events);
      else 
         *event_list = new_events;
   }

   /* commlib error during first synchronous get_event_list() */
   if (sync && ret) {
      DEXIT;
      return ret;
   }
      
   /* send an ack to the qmaster for all received events */
   if (!sync) {
      if (sge_send_ack_to_qmaster(0, ACK_EVENT_DELIVERY, next_event-1, lGetUlong(ec, EV_id))) {
         WARNING((SGE_EVENT, MSG_COMMD_FAILEDTOSENDACKEVENTDELIVERY ));
      }
      else
         DPRINTF(("Sent ack for all events lower or equal %d\n",
                 (next_event-1)));
   }

   DEXIT;
   return 0;
}


/*----------------------------------------------------------------------
 * ck_event_number()
 *
 *   tests list of events if it
 *  contains right numbered events
 *
 *  events with lower numbers get trashed 
 *
 *  In cases the master has added no
 *  new events in the event list 
 *  and the acknowledge we sent was lost
 *  also a list with events lower 
 *  than "waiting_for" is correct.
 *  But the number of the last event must be 
 *  at least "waiting_for"-1.
 *
 *  in extreme cases it may be possible 
 *  that 
 *  if at least the last event in the event 
 *  list is in the right range
 *
 *  on success *waiting_for will
 *  contain the next number we wait for
 *
 *  on failure *waiting_for gets not 
 *  changed and if wrong_number is not
 *  NULL *wrong_number contains the
 *  wrong number we got
 *
 *
 *  
 *  0 success
 *  -1 failure
 *----------------------------------------------------------------------*/
static u_long32 ck_event_number(
lList *lp,
u_long32 *waiting_for,
u_long32 *wrong_number 
) {
   lListElem *ep, *tmp;
   u_long32 i, j;
   int skipped;
  
   DENTER(TOP_LAYER, "ck_event_number");

   i = *waiting_for;

   if (!lp || !lGetNumberOfElem(lp)) {
      /* got a dummy event list for alive protocol */
      DEXIT;
      return 0;
   }
   DPRINTF(("Checking %d events (" u32"-"u32 ") while waiting for #"u32"\n",
         lGetNumberOfElem(lp), 
         lGetUlong(lFirst(lp), ET_number),
         lGetUlong(lLast(lp), ET_number),
         i));
 
   /* ensure number of last event is "waiting_for"-1 or higher */ 
   if ((j=lGetUlong(lLast(lp), ET_number)) < i-1) {
      /* error in event numbers */
      /* could happen if the order of two event messages was exchanged */
      if (wrong_number)
         *wrong_number = j;

      ERROR((SGE_EVENT, MSG_EVENT_HIGHESTEVENTISXWHILEWAITINGFORY_UU , 
                  u32c(j), u32c(i)));
      /* 
      DEXIT;
      return -1;
      */
   }

   /* ensure number of first event is lower or equal "waiting_for" */
   if ((j=lGetUlong(lFirst(lp), ET_number)) > i) {
      /* error in event numbers */
      if (wrong_number)
         *wrong_number = j;
      ERROR((SGE_EVENT, MSG_EVENT_SMALLESTEVENTXISGRTHYWAITFOR_UU,
               u32c(j), u32c(i)));
      DEXIT;
      return -1;
   }

   /* skip leading events till event number is "waiting_for"
      or there are no more events */ 
   skipped = 0;
   ep = lFirst(lp);
   while (ep && lGetUlong(ep, ET_number) < i) {
      tmp = lNext(ep);
      lRemoveElem(lp, ep);
      ep = tmp;
      skipped++;
   }
   if (skipped)
      DPRINTF(("Skipped %d events\n", skipped));

   /* ensure number of events increase */
   for_each (ep, lp) {
      if ((j=lGetUlong(ep, ET_number)) != i++) {
         /* 
            do not change waiting_for because 
            we still wait for this number 
         */
         ERROR((SGE_EVENT, MSG_EVENT_EVENTSWITHNOINCREASINGNUMBERS ));
         if (wrong_number)
            *wrong_number = j;
         DEXIT;
         return -1;
      }
   }
 
   /* that's the new number we wait for */
   *waiting_for = i;

   DEXIT;
   return 0;
}


/*-----------------------------------------------------------------------*
 * get_event_list
 *
 * returns   0 on success
 *           commlib error (always positive)
 *-----------------------------------------------------------------------*/
static int get_event_list(
int sync,
lList **report_list 
) {
   int tag, ret;
   u_short id;
   sge_pack_buffer pb;

   DENTER(TOP_LAYER, "get_event_list");

   id = 0;
   tag = TAG_REPORT_REQUEST;
   /* FIX_CONST */
   ret = sge_get_any_request((char*)sge_get_master(0), 
                             (char*)prognames[QMASTER], &id, &pb, &tag, sync);

   if (ret == 0) {
      if (cull_unpack_list(&pb, report_list)) {
         clear_packbuffer(&pb);
         ERROR((SGE_EVENT, MSG_LIST_FAILEDINCULLUNPACKREPORT ));
         DEXIT;
         return -1;
      }
      clear_packbuffer(&pb);
   }
   
   DPRINTF(("commlib returns %s (%d)\n", cl_errstr(ret), ret));

   DEXIT;
   return ret;
}
