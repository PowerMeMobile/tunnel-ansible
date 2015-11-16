/*
                Copyright (C) Dialogic Corporation 1999-2011. All Rights Reserved.

 Name:          mtr.c

 Description:   Simple responder for MTU (MAP test utility)
                This program responds to an incoming dialogue received
                from the MAP module.

                The program receives:
                        MAP-OPEN-IND
                        service indication
                        MAP-DELIMITER-IND

                and it responds with:
                        MAP-OPEN-RSP
                        service response
                        MAP-CLOSE-IND

                The following services are handled:
                        MAP-FORWARD-SHORT-MESSAGE
                        MAP-SEND-IMSI
                        MAP-SEND-ROUTING-INFO-FOR-GPRS
                        MAP-MT-FORWARD-SHORT-MESSAGE
                        MAP-SEND-ROUTING-INFO-FOR-SMS
                        MAP-PROCESS-UNSTRUCTURED-SS
                        MAP-UNSTRUCTURED-SS-REQ

 Functions:     main

 -----  ---------   ---------------------------------------------
 Issue    Date                       Changes
 -----  ---------   ---------------------------------------------
   A    11-Mar-99   - Initial code.
   1    16-Feb-00   - Support for multiple dialogues
                    - Recovers and prints out short message
   2    03-May-00   - Corrected problem with instance.
   3    10-Aug-01   - Added handling for SEND-ROUTING-INFO-FOR-GPRS
                      and SEND-IMSI.
   4    20-Jan-06   - Include reference to Intel Corporation in file header
   5    13-Dec-06   - Change to use of Dialogic Corporation copyright.
   6    30-Sep-09   - Support for USSD and other MAP services.
   7    24-Jun-10   - Corrected header length issue in print_sh_msg().
   8    22-Nov-10   - Add support for ATI Request, USSD-REQ.
   9    11-Jan-11   - Addition of dialog termination mode.
   10   23-Aug-11   - Update return type of MTR_get_msisdn()
                    - Only check for MSISDN for ATI Req.

 */

#include <stdio.h>
#include <string.h>

#include "system.h"
#include "msg.h"
#include "sysgct.h"
#include "map_inc.h"
#include "mtr.h"
#include "pack.h"

/*
 * Prototypes for local functions:
 */
int MTR_process_map_msg(MSG *m);
int MTR_cfg(u8 _mtr_mod_id, u8 _map_mod_id, u8 _trace_mod_id,u8 _dlg_term_mode);
int MTR_set_default_term_mode(u8 new_term_mode);

static int init_resources(void);
static u16 MTU_def_alph_to_str(u8 *da_octs, u16 da_olen, u16 da_num,
                                 char *ascii_str, u16 max_strlen);
static int print_sh_msg(MSG *m);
static dlg_info *get_dialogue_info(u16 dlg_id);
static int MTR_trace_msg(char *prefix, MSG *m);
static int MTR_send_msg(u16 instance, MSG *m);
static int MTR_send_OpenResponse(u16 mtr_map_inst, u16 dlg_id, u8 result);
static int MTR_ForwardSMResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendImsiResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendRtgInfoGprsResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSRequest (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSResponse (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_ProcessUnstructuredSSReqRsp (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_UnstructuredSSNotifyRsp (u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_send_MapClose(u16 mtr_map_inst, u16 dlg_id, u8 method);
static int MTR_send_Abort(u16 mtr_map_inst, u16 dlg_id, u8 reason);
static int MTR_send_Delimit(u16 mtr_map_inst, u16 dlg_id);
static int MTR_get_invoke_id(u8 *pptr, u16 plen);
static int MTR_get_applic_context(u8 *pptr, u16 plen, u8 *dst, u16 dstlen);
static u8  MTR_get_msisdn(u8 *pptr, u16 plen, u8 *dst, u16 dstlen);
static int MTR_get_sh_msg(u8 *pptr, u16 plen, u8 *dst, u16 dstlen);
static int MTR_MT_ForwardSMResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_SendRtgInfoSmsResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);
static int MTR_Send_ATIResponse(u16 mtr_map_inst, u16 dlg_id, u8 invoke_id);

/*
 * Static data:
 */
static dlg_info dialogue_info[MAX_NUM_DLGS];    /* Dialog_info */
static u8 mtr_mod_id;                           /* Module id of this task */
static u8 mtr_map_id;                           /* Module id for all MAP requests */
static u8 mtr_trace;                            /* Controls trace requirements */
static u8 mtr_default_dlg_term_mode;            /* Controls which end terminates a dialog */

#define MTR_ATI_RSP_SIZE         (8)
#define MTR_ATI_RSP_NUM_OF_RSP   (8)
static u8 mtr_ati_rsp_data[MTR_ATI_RSP_NUM_OF_RSP][MTR_ATI_RSP_SIZE] =
{
    {0x14, 0x10, 0x00, 0x00, 0x80, 0x00, 0x00, 0x14},     //0
    {0x14, 0x20, 0x00, 0x00, 0x70, 0x00, 0x00, 0x14},     //1
    {0x14, 0x30, 0x00, 0x00, 0x60, 0x00, 0x00, 0x14},     //2
    {0x14, 0x40, 0x00, 0x00, 0x50, 0x00, 0x00, 0x14},     //3
    {0x14, 0x50, 0x00, 0x00, 0x40, 0x00, 0x00, 0x14},     //4
    {0x14, 0x60, 0x00, 0x00, 0x30, 0x00, 0x00, 0x14},     //5
    {0x14, 0x70, 0x00, 0x00, 0x20, 0x00, 0x00, 0x14},     //6
    {0x14, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x14}      //7
};
/*
 * MTU_def_alph_to_str()
 * Returns the number of ascii characters formatted into the
 * ascii string. Returns zero if this could not be done.
 */
u16 MTU_def_alph_to_str(da_octs, da_olen, da_num, ascii_str, max_strlen)
  u8   *da_octs;      /* u8 array from which the deft alph chars are recoverd */
  u16   da_olen;      /* The formatted octet length of da_octs  */
  u16   da_num;       /* The number of formatted characters in the array */
  char *ascii_str;    /* location into which chars are written */
  u16   max_strlen;   /* The max space available for the ascii_str */
{
  char *start_ascii_str;   /* The first char */
  u16  i;                  /* The bit position along the da_octs */

  start_ascii_str = ascii_str;

  if ((da_olen * 8) > ((max_strlen + 1) * 7))
    return(0);

  if (  ( (da_num * 7)  >   (da_olen      * 8) )
     || ( (da_num * 7) <= ( (da_olen - 1) * 8) ) )
  {
    /*
     * The number of digits does not agree with the size of the string
     */
    return (0);
  }
  else
  {
    for (i=0; i < da_num; i++)
    {
      *ascii_str++ = DEF2ASCII(unpackbits(da_octs, i * 7, 7));
    }

    *ascii_str++ = '\0';

    return((u16)(ascii_str - start_ascii_str));
  }
}

/*
 * print_sh_msg()
 *
 * prints a received short message
 *
 * Always returns zero
 *
 */
int print_sh_msg(m)
  MSG *m;
{
  u8 *pptr;                     /* Parameter pointer */
  u16 plen;                     /* length of primitive data */
  u16 msg_len;                  /* Number of characters in short message */
  u8  raw_SM[MAX_SM_SIZE];      /* Buffer for holding raw SM */
  char ascii_SM[MAX_SM_SIZE];   /* Buffer for holding ascii SM */
  u8 num_semi_oct;              /* number of encoded useful semi-octets */
  u8 num_dig_bytes;             /* number of bytes of digits */
  u8 tot_header_len = SIZE_UI_HEADER_FIXED;  /* start off with the fixed part */


  pptr = get_param(m);
  plen = MTR_get_sh_msg(pptr, m->len, raw_SM, MAX_SM_SIZE);

  /* calc size of TP-OA in bytes*/
  num_semi_oct = raw_SM[1];
  num_dig_bytes = (num_semi_oct/2) + (num_semi_oct % 2);

  tot_header_len += num_dig_bytes; /* now total header length */

  plen -= tot_header_len;
  msg_len = raw_SM[tot_header_len - 1];

  printf("MTR Rx: Short Message User Information:\n");
  if(MTU_def_alph_to_str(raw_SM + tot_header_len, plen, msg_len,
                      ascii_SM, MAX_SM_SIZE) > 0)
    printf("MTR Rx: %s\n",ascii_SM);
  else
    printf("MTR Rx: (error decoding)\n");
  return(0);
}

/*
 * mtr_ent
 *
 * Waits in a continuous loop responding to any received
 * forward SM request with a forward SM response.
 *
 * Never returns.
 */
int mtr_ent(mtr_id, map_id, trace, dlg_term_mode)
  u8 mtr_id;       /* Module id for this task */
  u8 map_id;       /* Module ID for MAP */
  u8 trace;        /* Trace requirements */
  u8 dlg_term_mode;/* Default termination mode */
{
  HDR *h;               /* received message */
  MSG *m;               /* received message */

  MTR_cfg(mtr_id, map_id, trace, dlg_term_mode);

  /*
   * Print banner so we know what's running.
   */
  printf("MTR MAP Test Responder (C) Dialogic Corporation 1999-2009. All Rights Reserved.\n");
  printf("===============================================================================\n\n");
  printf("MTR mod ID - 0x%02x; MAP module Id 0x%x; Termination Mode 0x%x\n", mtr_mod_id, mtr_map_id, dlg_term_mode);
  if ( mtr_trace == 0 )
    printf(" Tracing disabled.\n\n");

  /*
   * Now enter main loop, receiving messages as they
   * become available and processing accordingly.
   */

  while (1)
  {
    /*
     * GCT_receive will attempt to receive messages
     * from the task's message queue and block until
     * a message is ready.
     */
    if ((h = GCT_receive(mtr_mod_id)) != 0)
    {
      m = (MSG *)h;
      MTR_trace_msg("MTR Rx:", m);
      switch (m->hdr.type)
      {
        case MAP_MSG_DLG_IND:
        case MAP_MSG_SRV_IND:
          MTR_process_map_msg(m);
        break;
      }

      /*
       * Once we have finished processing the message
       * it must be released to the pool of messages.
       */
      relm(h);
    }
  }
  return(0);
}

/*
 * Can be used to configure and initialise mtr
 */
int MTR_cfg(
  u8 _mtr_mod_id,
  u8 _map_mod_id,
  u8 _trace_mod_id,
  u8 _dlg_term_mode
  ){
  mtr_mod_id = _mtr_mod_id;
  mtr_map_id = _map_mod_id;
  mtr_trace = _trace_mod_id;
  mtr_default_dlg_term_mode = _dlg_term_mode;

  init_resources();
  return (0);
}

/*
 * Can be used to configure new termination mode
 */
int MTR_set_default_term_mode(
  u8 new_mode
  ){
    mtr_default_dlg_term_mode = new_mode;

    return (0);
  }

/*
 * Get Dialogue Info
 *
 * Returns pointer to dialogue info or 0 on error.
 */
dlg_info *get_dialogue_info(dlg_id)
  u16 dlg_id;               /* Dlg ID of the incoming message 0x800a perhaps */
{
  u16 dlg_ref;              /* Internal Dlg Ref, 0x000a perhaps */

  if (!(dlg_id & 0x8000) )
  {
    if (mtr_trace)
      printf("MTR Rx: Bad dialogue id: Outgoing dialogue id, dlg_id == %x\n",dlg_id);
    return 0;
  }
  else
  {
    dlg_ref = dlg_id & 0x7FFF;
  }

  if ( dlg_ref >= MAX_NUM_DLGS )
  {
    if (mtr_trace)
      printf("MTR Rx: Bad dialogue id: Out of range dialogue, dlg_id == %x\n",dlg_id);
    return 0;
  }
  return &dialogue_info[dlg_ref];
}


/*
 * MTR_process_map_msg
 *
 * Processes a received MAP primitive message.
 *
 * Always returns zero.
 */
int MTR_process_map_msg(m)
  MSG *m;                       /* Received message */
{
  u16  dlg_id;                  /* Dialogue id */
  u8   ptype;                   /* Parameter Type */
  u8   *pptr;                   /* Parameter Pointer */
  dlg_info *dlg_info;           /* State info for dialogue */
  u8   send_abort;              /* Set if abort to be generated */
  int  invoke_id;               /* Invoke id of received srv req */

  pptr = get_param(m);
  ptype = *pptr;

  dlg_id = m->hdr.id;
  send_abort = 0;

  /*
   * Get state information associated with this dialogue
   */
  dlg_info = get_dialogue_info(dlg_id);

  if (dlg_info == 0)
    return 0;

  switch (dlg_info->state)
  {
    case MTR_S_NULL :
      /*
       * Null state.
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_DLG_IND :
          switch (ptype)
          {
            case MAPDT_OPEN_IND :
              /*
               * Open indication indicates that a request to open a new
               * dialogue has been received
               */
              if ( mtr_trace)
                printf("MTR Rx: Received Open Indication\n");

              /*
               * Save application context and MAP instance
               * We don't do actually do anything further with it though.
               */
              dlg_info->map_inst = (u16)GCT_get_instance((HDR*)m);
              dlg_info->ac_len =(u8)MTR_get_applic_context(pptr, m->len,
                                                           dlg_info->app_context,
                                                           MTR_MAX_AC_LEN);
              /*
               * Set the termination mode based on the current default
               */
              dlg_info->term_mode = mtr_default_dlg_term_mode;

              if (dlg_info->ac_len != 0)
              {
                /*
                 * Respond to the OPEN_IND with OPEN_RSP and wait for the
                 * service indication
                 */
                MTR_send_OpenResponse(dlg_info->map_inst, dlg_id, MAPRS_DLG_ACC);
                dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
              }
              else
              {
                /*
                 * We do not have a proper Application Context - abort
                 * the dialogue
                 */
                send_abort = 1;
              }
              break;

            default :
              /*
               * Unexpected event - Abort the dialogue.
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue.
           */
          send_abort = 1;
          break;
      }
      break;

    case MTR_S_WAIT_FOR_SRV_PRIM :
      /*
       * Waiting for service primitive
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_SRV_IND :
          /*
           * Service primitive indication
           */
          switch (ptype)
          {
            case MAPST_FWD_SM_IND :
            case MAPST_SEND_IMSI_IND :
            case MAPST_SND_RTIGPRS_IND :
            case MAPST_MT_FWD_SM_IND:
            case MAPST_SND_RTISM_IND:
            case MAPST_PRO_UNSTR_SS_REQ_IND :
            case MAPST_UNSTR_SS_REQ_CNF :
            case MAPST_UNSTR_SS_REQ_IND :
            case MAPST_UNSTR_SS_NOTIFY_IND :
            case MAPST_ANYTIME_INT_IND :
              if (mtr_trace)
              {
                switch (ptype)
                {
                  case MAPST_FWD_SM_IND :
                    printf("MTR Rx: Received Forward Short Message Indication\n");
                    break;
                  case MAPST_MT_FWD_SM_IND :
                    printf("MTR Rx: Received MT Forward Short Message Indication\n");
                    break;
                  case MAPST_SEND_IMSI_IND :
                    printf("MTR Rx: Received Send IMSI Indication\n");
                    break;
                  case MAPST_SND_RTIGPRS_IND :
                    printf("MTR Rx: Received Send Routing Info for GPRS Indication\n");
                    break;
                  case MAPST_SND_RTISM_IND :
                    printf("MTR Rx: Received Send Routing Info for SMS Indication\n");
                    break;
                  case MAPST_PRO_UNSTR_SS_REQ_IND :
                    printf("MTR Rx: Received ProcessUnstructuredSS-Indication\n");
                    break;
                  case MAPST_UNSTR_SS_REQ_CNF :
                    printf("MTR Rx: Received UnstructuredSS-Req-Confirmation\n");
                    break;
                  case MAPST_UNSTR_SS_REQ_IND :
                    printf("MTR Rx: Received UnstructuredSS-Indication\n");
                    break;
                  case MAPST_UNSTR_SS_NOTIFY_IND :
                    printf("MTR Rx: Received UnstructuredSS-Notify Indication\n");
                    break;
                  case MAPST_ANYTIME_INT_IND :
                    printf("MTR Rx: Received AnyTimeInterrogation Indication\n");
                    break;
                  default :
                    send_abort = 1;
                  break;

                }
              }

              /*
               * Recover invoke id. The invoke id is used
               * when sending the Forward short message response.
               */
              invoke_id = MTR_get_invoke_id(get_param(m), m->len);

              /*
               * If recovery of the invoke id succeeded, save invoke id and
               * primitive type and change state to wait for the delimiter.
               */
              if (invoke_id != -1)
              {
                dlg_info->invoke_id = (u8)invoke_id;
                dlg_info->ptype = ptype;

                /*
                 * Store MSISDN if available for use with ATI Response test data lookup
                 */
                if (ptype == MAPST_ANYTIME_INT_IND)
                  dlg_info->msisdn_len = MTR_get_msisdn(pptr, m->len, dlg_info->msisdn, MTR_MAX_MSISDN_SIZE);

                if ((mtr_trace) && (ptype == MAPST_FWD_SM_IND || ptype == MAPST_MT_FWD_SM_IND))
                  print_sh_msg(m);
                dlg_info->state = MTR_S_WAIT_DELIMITER;
                break;
              }
              else
              {
                printf("MTR RX: No invoke ID included in the message\n");
              }
              break;

            default :
                send_abort = 1;
              break;
          }
          break;

        case MAP_MSG_DLG_IND :
          /*
           * Dialogue indication - we were not expecting this!
           */
          switch (ptype)
          {
            case MAPDT_NOTICE_IND :
              /*
               * MAP-NOTICE-IND indicates some kind of error. Close the
               * dialogue and idle the state machine.
               */
              if (mtr_trace)
                printf("MTR Rx: Received Notice Indication\n");
              /*
               * Now send Map Close and go to idle state.
               */
              MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
              dlg_info->state = MTR_S_NULL;
              send_abort = 0;
              break;

            case MAPDT_CLOSE_IND :
              /*
               * Close indication received.
               */
              if (mtr_trace)
                printf("MTR Rx: Received Close Indication\n");
              dlg_info->state = MTR_S_NULL;
              send_abort = 0;
              break;

            default :
              /*
               * Unexpected event - Abort the dialogue.
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue.
           */
          send_abort = 1;
          break;
      }
      break;

    case MTR_S_WAIT_DELIMITER :
      /*
       * Wait for delimiter.
       */
      switch (m->hdr.type)
      {
        case MAP_MSG_DLG_IND :

          switch (ptype)
          {
            case MAPDT_DELIMITER_IND :
              /*
               * Delimiter indication received. Now send the appropriate
               * response depending on the service primitive that was received.
               */
              if (mtr_trace)
                printf("MTR Rx: Received delimiter Indication\n");

              switch (dlg_info->ptype)
              {
                case MAPST_FWD_SM_IND :
                  MTR_ForwardSMResponse(dlg_info->map_inst, dlg_id,
                                        dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_MT_FWD_SM_IND :
                  MTR_MT_ForwardSMResponse(dlg_info->map_inst, dlg_id,
                                           dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_SEND_IMSI_IND :
                  MTR_SendImsiResponse(dlg_info->map_inst, dlg_id,
                                       dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_SND_RTIGPRS_IND :
                  MTR_SendRtgInfoGprsResponse(dlg_info->map_inst, dlg_id,
                                              dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_SND_RTISM_IND :
                  MTR_SendRtgInfoSmsResponse(dlg_info->map_inst, dlg_id,
                                             dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_PRO_UNSTR_SS_REQ_IND :
                  MTR_Send_UnstructuredSSRequest (dlg_info->map_inst, dlg_id,
                                              dlg_info->invoke_id);
                  MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                                  dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
                  send_abort = 0;
                  break;
                case MAPST_UNSTR_SS_REQ_CNF :
                  MTR_Send_ProcessUnstructuredSSReqRsp (dlg_info->map_inst, dlg_id,
                                              dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
                case MAPST_UNSTR_SS_REQ_IND :
                  MTR_Send_UnstructuredSSResponse (dlg_info->map_inst, dlg_id,
                                              dlg_info->invoke_id);
                  MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                  dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
                  send_abort = 0;
                  break;
                case MAPST_UNSTR_SS_NOTIFY_IND :
                  MTR_Send_UnstructuredSSNotifyRsp (dlg_info->map_inst, dlg_id,
                                              dlg_info->invoke_id);

                  if ((dlg_info->term_mode == DLG_TERM_MODE_AUTO) ||
                      (dlg_info->term_mode == DLG_TERM_MODE_LOCAL_CLOSE))
                  {
                    MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                    dlg_info->state = MTR_S_NULL;
                  }
                  else
                  {
                    MTR_send_Delimit(dlg_info->map_inst, dlg_id);
                    dlg_info->state = MTR_S_WAIT_FOR_SRV_PRIM;
                  }
                  send_abort = 0;

                  break;

                case MAPST_ANYTIME_INT_IND:
                  MTR_Send_ATIResponse(dlg_info->map_inst, dlg_id,
                                        dlg_info->invoke_id);
                  MTR_send_MapClose(dlg_info->map_inst, dlg_id, MAPRM_normal_release);
                  dlg_info->state = MTR_S_NULL;
                  send_abort = 0;
                  break;
              }
                break;

            default :
              /*
               * Unexpected event - Abort the dialogue
               */
              send_abort = 1;
              break;
          }
          break;

        default :
          /*
           * Unexpected event - Abort the dialogue
           */
            send_abort = 1;
          break;
      }
      break;
  }
  /*
   * If an error or unexpected event has been encountered, send abort and
   * return to the idle state.
   */
    if (send_abort)
  {
    MTR_send_Abort (dlg_info->map_inst, dlg_id, MAPUR_procedure_error);
    dlg_info->state = MTR_S_NULL;

  }
  return(0);
}

/******************************************************************************
 *
 * Functions to send primitive requests to the MAP module
 *
 ******************************************************************************/

/*
 * MTR_send_OpenResponse
 *
 * Sends an open response to MAP
 *
 * Returns zero or -1 on error.
 */
static int MTR_send_OpenResponse(instance, dlg_id, result)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  result;          /* Result (accepted/rejected) */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Open Response\n");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id,
                     NO_RESPONSE, (u16)(7 + dlg_info->ac_len))) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;
    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Open response
     * Parameter name   = result_tag
     * Parameter length = 1
     * Parameter value  = result
     * Parameter name   = applic_context_tag
     * parameter length = len
     * parameter data   = applic_context
     * EOC_tag
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_OPEN_RSP;
    pptr[1] = MAPPN_result;
    pptr[2] = 0x01;
    pptr[3] = result;
    pptr[4] = MAPPN_applic_context;
    pptr[5] = (u8)dlg_info->ac_len;
    memcpy((void*)(pptr+6), (void*)dlg_info->app_context, dlg_info->ac_len);
    pptr[6+dlg_info->ac_len] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_ForwardSMResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_ForwardSMResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Forward SM Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Forward SM response
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_FWD_SM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_SendRtgInfoGprsResponse
 *
 * Sends a send routing info for GPRS response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendRtgInfoGprsResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send Routing Info for GPRS Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 12)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send Routing Info for GPRS response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = SGSN address
     * Parameter length = 4
     * Parameter value:
     *   1st octet:  address type = IPv4; addres length = 4
     *   remaining octets = 193.195.185.113
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SND_RTIGPRS_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_sgsn_address;
    pptr[5] = 5;
    pptr[6] = 4;
    pptr[7] = 193;
    pptr[8] = 195;
    pptr[9] = 185;
    pptr[10] = 113;
    pptr[11] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_SendImsiResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendImsiResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send IMSI Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 14)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = send IMSI response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Primitive name = IMSI
     * Parameter length = 7
     * Parameter value = 60802678000454

     * Parameter name = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SEND_IMSI_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_imsi;
    pptr[5] = 7;
    pptr[6] = 0x06;
    pptr[7] = 0x08;
    pptr[8] = 0x62;
    pptr[9] = 0x87;
    pptr[10] = 0x00;
    pptr[11] = 0x40;
    pptr[12] = 0x45;
    pptr[13] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_MT_ForwardSMResponse
 *
 * Sends a MT forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_MT_ForwardSMResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending MT Forward SM Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = MT Forward SM response
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_MT_FWD_SM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_SendRtgInfoSmsResponse
 *
 * Sends a send routing info for SMS response to MAP.
 *
 * Always returns zero.
 */
static int MTR_SendRtgInfoSmsResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send Routing Info for SMS Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 23)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send Routing Info for SMS response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = IMSI
     * Parameter length = 7
     * Parameter value:
     *   remaining octets = 60802678000454
     *   TBCD digits
     *
     * Parameter name = MSC Number
     * Parameter length = 7
     * Parameter value:
     *   remaining octets = 375290000001
	 *   TBCD digits
     *
     * Parameter name   = terminator (0x00)
     */
    pptr = get_param(m);
    pptr[0] = MAPST_SND_RTISM_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_imsi;
    pptr[5] = 7;
    pptr[6] = 0x06; // 60802678000454
    pptr[7] = 0x08;
    pptr[8] = 0x62;
    pptr[9] = 0x87;
    pptr[10] = 0x00;
    pptr[11] = 0x40;
    pptr[12] = 0x45;
    pptr[13] = MAPPN_msc_num;
    pptr[14] = 7;
    pptr[15] = 0x91; // ton/npi = 1/1
    pptr[16] = 0x73; // 375290000002
    pptr[17] = 0x25;
    pptr[18] = 0x09;
    pptr[19] = 0x00;
    pptr[20] = 0x00;
    pptr[21] = 0x20;
    pptr[22] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_UnstructuredSSRequest
 * Formats and sends an UnstructuredSS-Request message
 * in response to a received ProcessUnstructuredSS-Request.
 */

 static int MTR_Send_UnstructuredSSRequest(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 47)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Send_UnstructuredSS-Request
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'XY Telecom <LF>'
     *                     '1. Balance <LF>'
     *                     '2. Texts Remaining'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_REQ;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 37;
    pptr[9] = 0xD8;
    pptr[10] = 0x2C;
    pptr[11] = 0x88;
    pptr[12] = 0x5A;
    pptr[13] = 0x66;
    pptr[14] = 0x97;
    pptr[15] = 0xC7;
    pptr[16] = 0xEF;
    pptr[17] = 0xB6;
    pptr[18] = 0x02;
    pptr[19] = 0x14;
    pptr[20] = 0x73;
    pptr[21] = 0x81;
    pptr[22] = 0x84;
    pptr[23] = 0x61;
    pptr[24] = 0x76;
    pptr[25] = 0xD8;
    pptr[26] = 0x3D;
    pptr[27] = 0x2E;
    pptr[28] = 0x2B;
    pptr[29] = 0x40;
    pptr[30] = 0x32;
    pptr[31] = 0x17;
    pptr[32] = 0x88;
    pptr[33] = 0x5A;
    pptr[34] = 0xC6;
    pptr[35] = 0xD3;
    pptr[36] = 0xE7;
    pptr[37] = 0x20;
    pptr[38] = 0x69;
    pptr[39] = 0xB9;
    pptr[40] = 0x1D;
    pptr[41] = 0x4E;
    pptr[42] = 0xBB;
    pptr[43] = 0xD3;
    pptr[44] = 0xEE;
    pptr[45] = 0x73;
    pptr[46] = 0x00;


    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_UnstructuredSSResponse
 * Formats and sends a UnstructuredSS Response message
 * in response to a received UnstructuredSS-Request-Ind.
 */

 static int MTR_Send_UnstructuredSSResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 27)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = MAPST_UNSTR_SS_IND_RSP
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'This is sample text'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 17;
    pptr[9] = 0x54;
    pptr[10] = 0x74;
    pptr[11] = 0x7a;
    pptr[12] = 0x0e;
    pptr[13] = 0x4a;
    pptr[14] = 0xcf;
    pptr[15] = 0x41;
    pptr[16] = 0xf3;
    pptr[17] = 0x70;
    pptr[18] = 0x1b;
    pptr[19] = 0xce;
    pptr[20] = 0x2e;
    pptr[21] = 0x83;
    pptr[22] = 0xe8;
    pptr[23] = 0x65;
    pptr[24] = 0x3c;
    pptr[25] = 0x1d;

    pptr[26] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/* MTR_Send_ProcessUnstructuredSSReq-Rsp
 * Formats and sends a ProcessUnstructuredSSReq-Rsp message
 * in response to a received UnstructuredSS-Request-CNF.
 */

 static int MTR_Send_ProcessUnstructuredSSReqRsp(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Send_UnstructuredSS-Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 26)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = ProcessUnstructuredSSReq-Rsp
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = USSD Data Coding Scheme
     * Parameter length = 1
     * Parameter value  = 'GSM default alphabet' 00001111
     *
     * Parameter name = USSD String
     *
     * Use the MTU function 'MTU_USSD_str_to_def_alph' to verify string encoding
     * USSD string below = 'Your balance = 350'
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_PRO_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = MAPPN_USSD_coding;
    pptr[5] = 0x01;
    pptr[6] = 0x0F;  /* USSD coding set to 'GSM default alphabet' 00001111 */
    pptr[7] = MAPPN_USSD_string;
    pptr[8] = 16;
    pptr[9] = 0xD9;
    pptr[10] = 0x77;
    pptr[11] = 0x5D;
    pptr[12] = 0x0E;
    pptr[13] = 0x12;
    pptr[14] = 0x87;
    pptr[15] = 0xD9;
    pptr[16] = 0x61;
    pptr[17] = 0xF7;
    pptr[18] = 0xB8;
    pptr[19] = 0x0C;
    pptr[20] = 0xEA;
    pptr[21] = 0x81;
    pptr[22] = 0x66;
    pptr[23] = 0x35;
    pptr[24] = 0x58;
    pptr[25] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}


/* MTR_Send_UnstructuredSSNotifyRsp-Rsp
 * Formats and sends a UnstructuredSS Notify-Rsp message
 * in response to a received UnstructuredSS-Notify-IND.
 */
 static int MTR_Send_UnstructuredSSNotifyRsp(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending UnstructuredSS-Notify Response\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     *
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPST_UNSTR_SS_REQ_RSP;
    pptr[1] = MAPPN_invoke_id;
    pptr[2] = 0x01;
    pptr[3] = invoke_id;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_Send_ATIResponse
 *
 * Sends a forward short message response to MAP.
 *
 * Always returns zero.
 */
static int MTR_Send_ATIResponse(instance, dlg_id, invoke_id)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  invoke_id;       /* Invoke_id */
{
  MSG  *m;                      /* Pointer to message to transmit */
  u8   *pptr;                   /* Pointer to a parameter */
  dlg_info *dlg_info;           /* Pointer to dialogue state information */
  u8   len = 0;
  u8   ati_index = 0;

  /*
   *  Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending ATI Response\n\r");

  /*
   * See if we have MSISDN to use as key into look-up.
   */
  if (dlg_info->msisdn_len != 0)
  {
    /*
     * Find the last digit and use as index into sample data.
     */
    if ((dlg_info->msisdn[dlg_info->msisdn_len - 1] >> 4) == 0xf)
        ati_index = ((dlg_info->msisdn[dlg_info->msisdn_len - 1]) & 0xf);
    else
        ati_index = (dlg_info->msisdn[dlg_info->msisdn_len - 1] >> 4);

    if (ati_index >= MTR_ATI_RSP_NUM_OF_RSP)
       ati_index = 0;
  }

  if (mtr_trace)
    printf("Using ATI sample data index %i\n", ati_index);

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_SRV_REQ, dlg_id, NO_RESPONSE, 15)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = send ATI response
     *
     * Parameter name   = invoke ID
     * Parameter length = 1
     * Parameter value  = invoke ID
     *
     * Parameter name = terminator
     */
    pptr = get_param(m);
    pptr[len++] = MAPST_ANYTIME_INT_RSP;
    pptr[len++] = MAPPN_invoke_id;
    pptr[len++] = 0x01;
    pptr[len++] = invoke_id;

    pptr[len++] = MAPPN_geog_info;
    pptr[len++] = 0x8;
    memcpy(&pptr[len], mtr_ati_rsp_data[ati_index], 8);

    /*
     * Length is always 8
     */
    len += 8;
    pptr[len++] = 0x00;



    /*
     * Now send the message
     */
    MTR_send_msg(instance, m);
  }
  return(0);
}

/*
 * MTR_send_MapClose
 *
 * Sends a Close message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_MapClose(instance, dlg_id, method)
  u16 instance;        /* Destination instance */
  u16 dlg_id;          /* Dialogue id */
  u8  method;          /* Release method */
{
  MSG  *m;                   /* Pointer to message to transmit */
  u8   *pptr;                /* Pointer to a parameter */
  dlg_info *dlg_info;        /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Close Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = release method tag
     * Parameter length = 1
     * Parameter value  = release method
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_CLOSE_REQ;
    pptr[1] = MAPPN_release_method;
    pptr[2] = 0x01;
    pptr[3] = method;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_Abort
 *
 * Sends an abort message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Abort(instance, dlg_id, reason)
  u16 instance;         /* Destination instance */
  u16 dlg_id;           /* Dialogue id */
  u8  reason;           /* user reason for abort */
{
  MSG  *m;              /* Pointer to message to transmit */
  u8   *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info;   /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending User Abort Request\n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Close Request
     * Parameter name   = user reason tag
     * Parameter length = 1
     * Parameter value  = reason
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_U_ABORT_REQ;
    pptr[1] = MAPPN_user_rsn;
    pptr[2] = 0x01;
    pptr[3] = reason;
    pptr[4] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_Delimit
 *
 * Sends a Delimit message to MAP.
 *
 * Always returns zero.
 */
static int MTR_send_Delimit(instance, dlg_id)
  u16 instance;         /* Destination instance */
  u16 dlg_id;           /* Dialogue id */
{
  MSG  *m;              /* Pointer to message to transmit */
  u8   *pptr;           /* Pointer to a parameter */
  dlg_info *dlg_info;   /* Pointer to dialogue state information */

  /*
   * Get the dialogue information associated with the dlg_id
   */
  dlg_info = get_dialogue_info(dlg_id);
  if (dlg_info == 0)
    return (-1);

  if (mtr_trace)
    printf("MTR Tx: Sending Delimit \n\r");

  /*
   * Allocate a message (MSG) to send:
   */
  if ((m = getm((u16)MAP_MSG_DLG_REQ, dlg_id, NO_RESPONSE, 5)) != 0)
  {
    m->hdr.src = mtr_mod_id;
    m->hdr.dst = mtr_map_id;

    /*
     * Format the parameter area of the message
     *
     * Primitive type   = Delimit Request
     * Parameter name   = user reason tag
     * Parameter length = 1
     * Parameter value  = reason
     * Parameter name   = terminator
     */
    pptr = get_param(m);
    pptr[0] = MAPDT_DELIMITER_REQ;
    pptr[1] = 0x00;

    /*
     * Now send the message
     */
    MTR_send_msg(dlg_info->map_inst, m);
  }
  return(0);
}

/*
 * MTR_send_msg sends a MSG. On failure the
 * message is released and the user notified.
 *
 * Always returns zero.
 */
static int MTR_send_msg(instance, m)
  u16   instance;       /* Destination instance */
  MSG   *m;             /* MSG to send */
{
  GCT_set_instance((unsigned int)instance, (HDR*)m);
  MTR_trace_msg("MTR Tx:", m);

  /*
   * Now try to send the message, if we are successful then we do not need to
   * release the message.  If we are unsuccessful then we do need to release it.
   */

  if (GCT_send(m->hdr.dst, (HDR *)m) != 0)
  {
    if (mtr_trace)
      fprintf(stderr, "*** failed to send message ***\n");
    relm((HDR *)m);
  }
  return(0);
}


/******************************************************************************
 *
 * Functions to recover parameters from received MAP format primitives
 *
 ******************************************************************************/

/*
 * MTR_get_invoke_id
 *
 * recovers the invoke id parameter from a parameter array
 *
 * Returns the recovered value or -1 if not found.
 */
static int MTR_get_invoke_id(pptr, plen)
  u8  *pptr;        /* First byte of received primitive data (type octet) */
  u16 plen;         /* length of primitive data */
{
  int  invoke_id;   /* Recovered invoke_id */
  u8   ptype;       /* Parameter type*/

  /*
   * Skip past primitive type
   */
  pptr++;
  plen --;
  invoke_id = -1;

  while (plen)
  {
    ptype = *pptr++;
    plen = *pptr++;

    if (ptype == MAPPN_invoke_id)
    {
      /*
       * Verify that invoke ID length is 1 octet
       */
      if (plen == 1)
      {
        invoke_id = (int)*pptr;
        break;
      }
    }
    /*
     * Advance to next parameter
     */
    pptr += plen;
  }
  return(invoke_id);
}

/*
 * MTR_get_applic_context
 *
 * Recovers the Application Context parameter from a parameter array
 *
 * Returns the length of parameter data recovered (-1 on failure).
 */
static int MTR_get_applic_context(pptr, plen, dst, dstlen)
  u8  *pptr;    /* First byte of received primitive data (type octet) */
  u16 plen;     /* length of primitive data */
  u8  *dst;     /* Start of destination for recovered ac */
  u16 dstlen;   /* Space available at dst */
{
  u8   ptype;   /* Parameter type */
  int  retval;  /* Return value */

  retval = -1;
  /*
   * Skip past primitive type
   */
  pptr++;
  plen --;

  while (plen)
  {
    ptype = *pptr++;
    plen = *pptr++;

    if (ptype == MAPPN_applic_context)
    {
      /*
       * Verify that there is sufficient space to store the parameter data
       */
      if (plen <= dstlen)
      {
        memcpy((void*)dst, (void*)pptr, plen);
        retval = plen;
        break;
      }
    }
    /*
     * Advance to next parameter
     */
    pptr += plen;
  }
  return(retval);
}

/*
 * MTR_get_msisdn
 *
 * Recovers the MSISDN parameter from a parameter array
 *
 * Returns the length of parameter data recovered (0 on failure).
 */
static u8 MTR_get_msisdn(pptr, plen, dst, dstlen)
  u8  *pptr;    /* First byte of received primitive data (type octet) */
  u16 plen;     /* length of primitive data */
  u8  *dst;     /* Start of destination for recovered param */
  u16 dstlen;   /* Space available at dst */
{
  u8   ptype;   /* Parameter type */
  u8  retval;   /* Return value */

  retval = 0;
  /*
   * Skip past primitive type
   */
  pptr++;
  plen --;

  while (plen)
  {
    ptype = *pptr++;
    plen = *pptr++;

    if (ptype == MAPPN_msisdn)
    {
      /*
       * Verify that there is sufficient space to store the parameter data
       */
      if (plen <= dstlen)
      {
        memcpy((void*)dst, (void*)pptr, plen);
        if (plen <= 0xff)
          retval = (u8) plen;
        else
          retval = 0;
        break;
      }
    }
    /*
     * Advance to next parameter
     */
    pptr += plen;
  }
  return(retval);
}

/*
 * MTR_get_sh_msg
 *
 * recovers the short message parameter from a parameter array
 *
 * Returns the length of the recovered data or -1 if error.
 */
static int MTR_get_sh_msg(pptr, plen, dst, dstlen)
  u8  *pptr;    /* First byte of received primitive data (type octet) */
  u16 plen;     /* length of primitive data */
  u8  *dst;     /* Start of destination for recovered SM */
  u16 dstlen;   /* Space available at dst */
{
  u8   ptype;   /* Parameter type*/
  int  retval;  /* return value */

  /*
   * Skip past primitive type
   */
  pptr++;
  plen --;
  retval = -1;

  while (plen)
  {
    ptype = *pptr++;
    plen = *pptr++;

    if (ptype == MAPPN_sm_rp_ui)
    {
      /*
       * Verify that Short Message length is no greater than MAX_SM_SIZE
       */
      if (  (plen <= MAX_SM_SIZE)
         && (plen <= dstlen) )
      {
        memcpy((void*)dst, (void*)pptr, plen);
        retval = plen;
        break;
      }
    }
    /*
     * Advance to next parameter
     */
    pptr += plen;
  }
  return(retval);
}

/*
 * MTR_trace_msg
 *
 * Traces (prints) any message as hexadecimal to the console.
 *
 * Always returns zero.
 */
static int MTR_trace_msg(prefix, m)
  char *prefix;
  MSG  *m;               /* received message */
{
  HDR   *h;              /* pointer to message header */
  int   instance;        /* instance of MAP msg received from */
  u16   mlen;            /* length of received message */
  u8    *pptr;           /* pointer to parameter area */

  /*
   * If tracing is disabled then return
   */
  if (mtr_trace == 0)
    return(0);

  h = (HDR*)m;
  instance = GCT_get_instance(h);
  printf("%s I%04x M t%04x i%04x f%02x d%02x s%02x", prefix, instance, h->type,
          h->id, h->src, h->dst, h->status);

  if ((mlen = m->len) > 0)
  {
    if (mlen > MAX_PARAM_LEN)
      mlen = MAX_PARAM_LEN;
    printf(" p");
    pptr = get_param(m);
    while (mlen--)
    {
      printf("%c%c", BIN2CH(*pptr/16), BIN2CH(*pptr%16));
      pptr++;
    }
  }
  printf("\n");
  return(0);
}

/*
 * init_resources
 *
 * Initialises all mtr system resources
 * This includes dialogue state information.
 *
 * Always returns zero
 *
 */
static int init_resources()
{
  int i;    /* for loop index */

  for (i=0; i<MAX_NUM_DLGS; i++)
  {
    dialogue_info[i].state = MTR_S_NULL;
    dialogue_info[i].term_mode = mtr_default_dlg_term_mode;
  }
  return (0);
}
