/**
 * @file
 * net interface sending queue.
 *
 * Verification using sylixos(tm) real-time operating system
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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
#ifndef LWIP_HDR_NETIF_IFQUEUE_H
#define LWIP_HDR_NETIF_IFQUEUE_H

/*
 * packet queue node
 */
struct pktn {
  struct pktn *next;
  struct pktn *prev;
  struct pbuf p;
};

/*
 * netif packet queue
 */
struct pktq {
  struct pktn *in;
  struct pktn *out;
  size_t max_size;
  size_t cur_size;
};

#ifdef __cplusplus
extern "C" {
#endif

void pktq_init(struct pktq *pktq, size_t max_size);
void pktq_deinit(struct pktq *pktq);
err_t pktq_put(struct pktq *pktq, struct pbuf *p);
struct pktn *pktq_get(struct pktq *pktq);
void pktq_free(struct pktq *pktq, struct pktn *pktn);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_HDR_NETIF_IFQUEUE_H */
