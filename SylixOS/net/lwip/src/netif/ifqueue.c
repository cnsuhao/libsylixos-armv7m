/**
 * @file
 * net interface sending queue.
 *
 * Verification using sylixos(tm) real-time operating system
 */

/*
 * Copyright (c) 2006-2014 SylixOS Group.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 * 
 * Author: Han.hui <sylixos@gmail.com>
 *
 */

#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/mem.h"
#include "lwip/tcpip.h"
#include "netif/ifqueue.h"

#include <stddef.h>

#define PKTN_ALIGN_SIZE     ROUND_UP(sizeof(struct pktn), sizeof(size_t))

/*
 * init a netif send queue
 */
void pktq_init (struct pktq *pktq, size_t max_size)
{
  if (pktq) {
    pktq->in = NULL;
    pktq->out = NULL;
    pktq->max_size = max_size;
    pktq->cur_size = 0;
  }
}

/*
 * deinit a netif send queue
 */
void pktq_deinit (struct pktq *pktq)
{
  struct pktn *pktn;
  
  if (pktq) {
    while (pktq->in) {
      pktn = pktq->in;
      pktq->in = pktq->in->next;
      mem_free(pktn);
    }
  }
}

/*
 * put a packet into packet queue (must using in netif send packet function)
 * IN LOCK_TCPIP_CORE() stats.
 */
err_t pktq_put (struct pktq *pktq, struct pbuf *p)
{
  struct pktn *pktn;
  
  if (pktq->cur_size + p->tot_len > pktq->max_size) {
    return ERR_BUF;
  }
  
  pktn = (struct pktn *)mem_malloc(PKTN_ALIGN_SIZE + p->tot_len);
  if (pktn == NULL) {
    return ERR_MEM;
  }
  
  pktn->p.next = NULL;
  pktn->p.payload = (void *)((u8_t *)pktn + PKTN_ALIGN_SIZE);
  pktn->p.tot_len = p->tot_len;
  pktn->p.len = p->tot_len;
  pktn->p.type = PBUF_ROM;
  pktn->p.flags = p->flags;
  pktn->p.ref = 1;
  
  pbuf_copy(&pktn->p, p);
  
  pktn->next = pktq->in;
  pktn->prev = NULL;
  
  if (pktq->in) {
    pktq->in->prev = pktn;
  }
  pktq->in = pktn;
  if (pktq->out == NULL) {
    pktq->out = pktn;
  }
  
  pktq->cur_size += p->tot_len;
  
  return ERR_OK;
}

/*
 * get a packet from packet queue
 */
struct pktn *pktq_get (struct pktq *pktq)
{
  return pktq->out;
}

/*
 * free a packet queue node
 */
void pktq_free (struct pktq *pktq, struct pktn *pktn)
{
  pktn = pktq->out;
  if (pktn) {
    LOCK_TCPIP_CORE();
    pktq->out = pktn->prev;
    pktq->cur_size -= pktn->p.tot_len;
    UNLOCK_TCPIP_CORE();
    mem_free(pktn);
  }
}

/*
 * end
 */
