/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

/**
 *
 * \file    pnfs_do_mount.c
 * \brief   Initialization functions : does a 'mount' to get session ids.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SOLARIS
#include "solaris_port.h"
#endif                          /* _SOLARIS */

#include <string.h>
#include <signal.h>


#ifdef _USE_GSSRPC
#include <gssrpc/rpc.h>
#else
#include <rpc/rpc.h>
#endif

#include "PNFS/LAYOUT4_NFSV4_1_FILES/pnfs_layout4_nfsv4_1_files.h"
#include "pnfs_nfsv41_macros.h"

#define PNFS_LAYOUTFILE_NB_OP_EXCHANGEID 2
#define PNFS_LAYOUTFILE_NB_OP_CREATESESSION 2

extern time_t ServerBootTime ;

/**
 *
 * pnfs_do_mount: establishes a NFSv4.1 session between a thread and a DS
 *
 * Establishes a NFSv4.1 session between a thread and a DS
 *
 * @param pnfsclient        [INOUT] pointer to the pnfsclient structure (client to the ds).
 * @param pnfs_ds_param     [IN]    pointer to pnfs data server configuration
 *
 * @return NFS4_OK if successful
 * @return a NFSv4 error (positive value) if failed.
 *
 */
int pnfs_do_mount( pnfs_client_t * pnfsclient,  pnfs_ds_parameter_t * pds_param ) 
{
  COMPOUND4args argnfs4;
  COMPOUND4res resnfs4;
  client_owner4 client_owner ;
  struct timeval timeout = { 25, 0 };

  char clientowner_name[MAXNAMLEN] ;  
  char server_owner_pad[PNFS_LAYOUTFILE_PADDING_LEN] ;
  uint32_t bitmap1[2] ;
  uint32_t bitmap2[2] ;

  nfs_argop4 argoparray_exchangeid[PNFS_LAYOUTFILE_NB_OP_EXCHANGEID];
  nfs_resop4 resoparray_exchangeid[PNFS_LAYOUTFILE_NB_OP_EXCHANGEID];

  nfs_argop4 argoparray_createsession[PNFS_LAYOUTFILE_NB_OP_CREATESESSION];
  nfs_resop4 resoparray_createsession[PNFS_LAYOUTFILE_NB_OP_CREATESESSION];

  if( !pnfsclient || !pds_param ) 
   return NFS4ERR_SERVERFAULT ;

  if( pnfsclient->rpc_client == NULL )
    return NFS4ERR_SERVERFAULT ;

  /* Setup 1 : EXCHANGEID */
  argnfs4.argarray.argarray_val = argoparray_exchangeid;
  resnfs4.resarray.resarray_val = resoparray_exchangeid;

  argnfs4.minorversion = 1;
  argnfs4.tag.utf8string_val = NULL;
  argnfs4.tag.utf8string_len = 0;
  argnfs4.argarray.argarray_len = 0;

  resoparray_exchangeid[0].nfs_resop4_u.opexchange_id.EXCHANGE_ID4res_u.eir_resok4.eir_state_protect.state_protect4_r_u.spr_mach_ops.spo_must_enforce.bitmap4_val = bitmap1 ;
  resoparray_exchangeid[0].nfs_resop4_u.opexchange_id.EXCHANGE_ID4res_u.eir_resok4.eir_state_protect.state_protect4_r_u.spr_mach_ops.spo_must_allow.bitmap4_val = bitmap2 ;
  resoparray_exchangeid[0].nfs_resop4_u.opexchange_id.EXCHANGE_ID4res_u.eir_resok4.eir_server_owner.so_major_id.so_major_id_val = server_owner_pad ;

  snprintf(clientowner_name, MAXNAMLEN, "GANESHA PNFS MDS Thread=(%u,%llu)", getpid(), (unsigned long long)pthread_self() );
  client_owner.co_ownerid.co_ownerid_len = strnlen( clientowner_name, MAXNAMLEN);
  client_owner.co_ownerid.co_ownerid_val = clientowner_name ;
  snprintf(client_owner.co_verifier, NFS4_VERIFIER_SIZE, "%x", (int)ServerBootTime);

  COMPOUNDV41_ARG_ADD_OP_EXCHANGEID( argnfs4, client_owner ) ;
  if( COMPOUNDV41_EXECUTE_SIMPLE( pnfsclient, argnfs4, resnfs4 ) != RPC_SUCCESS )
   {
      return NFS4ERR_IO ; /* @todo: For wanting of something more appropriate */
   }

  /* Check for compound status */
  if( resnfs4.status != NFS4_OK )
    return resnfs4.status ;

  /* Step 2 : CREATE_SESSION */
  argnfs4.argarray.argarray_val = argoparray_createsession;
  resnfs4.resarray.resarray_val = resoparray_createsession;

  argnfs4.minorversion = 1;
  argnfs4.tag.utf8string_val = NULL;
  argnfs4.tag.utf8string_len = 0;
  argnfs4.argarray.argarray_len = 0;
  
  COMPOUNDV41_ARG_ADD_OP_CREATESESSION( argnfs4, resoparray_exchangeid[0].nfs_resop4_u.opexchange_id.EXCHANGE_ID4res_u.eir_resok4.eir_clientid  );
  if( COMPOUNDV41_EXECUTE_SIMPLE( pnfsclient, argnfs4, resnfs4 ) != RPC_SUCCESS )
   {
      return NFS4ERR_IO ; /* @todo: For wanting of something more appropriate */
   }

  /* Keep the session for later use */
  memcpy( &pnfsclient->session, 
          &resoparray_createsession[0].nfs_resop4_u.opcreate_session.CREATE_SESSION4res_u.csr_resok4.csr_sessionid, 
          NFS4_SESSIONID_SIZE ) ;

  /* Keep the sequence as well */
  pnfsclient->sequence = resoparray_createsession[0].nfs_resop4_u.opcreate_session.CREATE_SESSION4res_u.csr_resok4.csr_sequence ;

  /* Check for compound status */
  if( resnfs4.status != NFS4_OK )
    return resnfs4.status ;

  return NFS4_OK ;
} /* pnfs_do_mount */