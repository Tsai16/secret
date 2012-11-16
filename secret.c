#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>
#include <sys/ioc_secret.h>
#include "secret.h"

/*
 * Function prototypes for the hello driver.
 */
FORWARD _PROTOTYPE( int secret_open,      (message *m) );
FORWARD _PROTOTYPE( int secret_close,     (message *m) );
FORWARD _PROTOTYPE( struct device * secret_prepare, (dev_t device) );
FORWARD _PROTOTYPE( int secret_transfer,  (endpoint_t endpt, int opcode,
                                          u64_t position, iovec_t *iov,
                                          unsigned int nr_req,
                                          endpoint_t user_endpt) );
FORWARD _PROTOTYPE( int secret_ioctl, (message *m_ptr) );

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( int sef_cb_lu_state_save, (int) );
FORWARD _PROTOTYPE( int lu_state_restore, (void) );

/* Entry points to the hello driver. */
PRIVATE struct chardriver hello_tab =
{
    secret_open,
    secret_close,
    secret_ioctl,
    secret_prepare,
    secret_transfer,
    nop_cleanup,
    nop_alarm,
    nop_cancel,
    nop_select,
    NULL
};


PRIVATE struct device hello_device;
PRIVATE int inUse = 0;
PRIVATE int curUser = -1;
PRIVATE char secret[SECRET_SIZE];
PRIVATE int curSize = 0; // current size  of secret


PRIVATE int secret_ioctl(message *m)
{
  struct ucred cred;
  int status;
  int res;
  int grantee;

  status = getnucred(m->USER_ENDPT, &cred);    
  res = sys_safecopyfrom(m->USER_ENDPT, (vir_bytes)m->IO_GRANT,
                             0, (vir_bytes)&grantee, sizeof(grantee), D);
                             
  if(m->REQUEST == SSGRANT){
    if(cred.uid == curUser) {
      curUser = grantee;
    }
    return OK;
  }
  
  return ENOTTY;
}

PRIVATE int secret_open(message *m)
{
  struct ucred cred;
  int status;

  status = getnucred(m->USER_ENDPT, &cred);  

  switch (m->COUNT) {
    case 2: // write
      if(inUse == 0) {
        inUse = 1;
        curUser = cred.uid;
        return OK;
      } else if (inUse && (cred.uid != curUser)) {
        return EACCES;
      } else {
        return ENOSPC;
      }
      break;

    case 4: // READ
      if (curUser == -1) {
        return OK;
      } else if(cred.uid == curUser) {
        curUser = -1;
        inUse = 0;
        return OK;
      } else {
        return EACCES;      
      }
      break;

    case 6: // READ/WRITE
      return EACCES;
      break;
      
    default:
      break;
  }  

  return OK;
}

PRIVATE int secret_close(message *m)
{
  if(inUse == 0) {
      memset(&secret[0], 0, sizeof(secret));
  }  
  return OK;
}

PRIVATE struct device * secret_prepare(dev_t UNUSED(dev))
{
    hello_device.dv_base = make64(0, 0);
    hello_device.dv_size = make64(SECRET_SIZE, 0);
    return &hello_device;
}

PRIVATE int secret_transfer(endpoint_t endpt, 
                           int opcode, 
                           u64_t position,
                           iovec_t *iov, 
                           unsigned nr_req, 
                           endpoint_t user_endpt)
{
    int bytes, ret;
    
    if (nr_req != 1) {
        /* This should never trigger for character drivers at the moment. */
        printf("HELLO: vectored transfer request, using first element only\n");
    }

    switch (opcode) {
      case DEV_GATHER_S: // Reading        
          bytes = curSize < iov->iov_size ?
                  curSize : iov->iov_size;
        break;
      
      case DEV_SCATTER_S: //Writing
        bytes = iov->iov_size;
        break;
    }
    /*
    printf("\tiov_size: %lu\t", iov->iov_size);   
    printf("curSize: %d\t", curSize);
    printf("bytes: %d\t", bytes);
    printf("secret: %s\n", secret);
    */
    
    if (bytes <= 0) {
        return OK;
    }
    switch (opcode) {
        case DEV_GATHER_S: // Reading
            inUse = 0;
            curSize -= bytes;    
            ret = sys_safecopyto(endpt, (cp_grant_id_t) iov->iov_addr, 0,
                                 (vir_bytes) secret, bytes, D);
            break;

        case DEV_SCATTER_S: //Writing
            inUse = 1;  
            curSize += bytes;    
            ret = sys_safecopyfrom(endpt, (cp_grant_id_t) iov->iov_addr, 0, 
                                  (vir_bytes) &secret, bytes, D);
            break;

        default:
            return EINVAL;
    }
    iov->iov_size -= bytes;
    return ret;
}

PRIVATE int sef_cb_lu_state_save(int UNUSED(state)) {

  /* Save the state. */
  ds_publish_u32("inUse", inUse, DSF_OVERWRITE);
  ds_publish_u32("curUser", curUser, DSF_OVERWRITE);
  ds_publish_u32("curSize", curSize, DSF_OVERWRITE);
  ds_publish_u32("secretSize", strlen(secret), DSF_OVERWRITE);
  ds_publish_str("secret", secret, DSF_OVERWRITE);  
  return OK;
}

PRIVATE int lu_state_restore() {

    /* Restore the state. */
    u32_t value;
    size_t secretSize;

    ds_retrieve_u32("secretSize", &value);
    ds_delete_u32("secretSize");
    secretSize = (int) value;

    ds_retrieve_str("secret", secret, secretSize);
    ds_delete_str("secret");

    ds_retrieve_u32("inUse", &value);
    ds_delete_u32("inUse");
    inUse = (int) value;

    ds_retrieve_u32("curSize", &value);
    ds_delete_u32("curSize");
    curSize = (int) value;

    ds_retrieve_u32("curUser", &value);
    ds_delete_u32("curUser");
    curUser = (int) value;
    
    printf("secret: %s\n", secret);
    printf("curuser: %d\n", curUser);
    printf("inUse: %d\n", inUse);

    return OK;
}

PRIVATE void sef_local_startup()
{
    /*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

PRIVATE int sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the hello driver. */
    int do_announce_driver = TRUE;

    switch(type) {
        case SEF_INIT_FRESH:
            //printf("%s", HELLO_MESSAGE);
        break;

        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;
            printf("I'm a new version!\n");
        break;

        case SEF_INIT_RESTART:
            printf("Hey, I've just been restarted!\n");
        break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        chardriver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

PUBLIC int main(void)
{
    /*
     * Perform initialization.
     */
    sef_local_startup();

    /*
     * Run the main loop.
     */
    chardriver_task(&hello_tab, CHARDRIVER_SYNC);
    return OK;
}


  /*
  printf("-------------\n");  
  printf("inUse: %d \t curUser: %d\n", inUse, curUser);
  printf("type: %d\t", m->m_type);
  printf("count: %d\t", (int)m->COUNT);
  printf("source: %d\t", m->m_source);
  printf("request: %d\n", m->REQUEST);
  printf("uid: %d\t\tgid: %d\t\tpid: %d\n", cred.uid, cred.gid,cred.pid);
  printf("-------------\n");  
  */


    //status = getnucred(user_endpt, &credentials);    
    //printf("original : %d\n", credentials.uid);
    //printf("current : %d\n", getuid());
    //printf("inUse: %d\n", inUse);
    //printf("iov->iov_size(%lu)\n", iov->iov_size);    // size of thing being written
