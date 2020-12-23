/*****************************************************************************
 *
 *   This file is provided under GPLv2 license.  When using or
 *   redistributing this file, you may do so under GPL v2 license.
 *
 *   GPL LICENSE SUMMARY
 *
 *     Copyright(c) 2007-2020 Intel Corporation. All rights reserved.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of version 2 of the GNU General Public License as
 *     published by the Free Software Foundation.

 *     This program is distributed in the hope that it will be useful, but
 *     WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *     General Public License for more details.

 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *     The full GNU General Public License is included in this distribution
 *     in the file called LICENSE.GPL.

 *     Contact Information:
 *     Intel Corporation
 *****************************************************************************/

#ifndef	_SYS_QAT_H
#define	_SYS_QAT_H

#include "cpa.h"
#include "dc/cpa_dc.h"
#include "lac/cpa_cy_sym.h"
#include "lac/cpa_cy_im.h"
#include "lac/cpa_cy_common.h"
#include "dataKVIO.h"
#include "dataVIO.h"
#include "qatInternals.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/completion.h>
#include <linux/atomic.h>

/*
 * The minimal and maximal buffer size which are not restricted in
 * the QAT hardware, but with the input buffer size between 4KB and
 * 128KB the hardware can provide the optimal performance.
 */
#define	QAT_MIN_BUF_SIZE	(4*1024)
#define	QAT_MAX_BUF_SIZE	(128*1024)

/*
 * fake CpaStatus used to indicate data was not compressible
 */
#define	CPA_STATUS_INCOMPRESSIBLE				(-127)

/**********************************************************************/
/*
 * VIO checksum types
 */
enum vio_checksum {
  VIO_CHECKSUM_INHERIT = 0,
  VIO_CHECKSUM_SHA256,
  VIO_CHECKSUM_SHA512,
#if !defined(__FreeBSD__)
  VIO_CHECKSUM_EDONR,
#endif
  VIO_CHECKSUM_FUNCTIONS
};

/**********************************************************************/

/*
 * inlined for performance
 */
static inline struct page *qat_mem_to_page(void *addr)
{
  if (!is_vmalloc_addr(addr)) {
    return (virt_to_page(addr));
  }
  return (vmalloc_to_page(addr));
}

/*
 * QAT memory contig allocation/free
 */
CpaStatus qat_mem_alloc_contig(void **pp_mem_addr, Cpa32U size_bytes);
void qat_mem_free_contig(void **pp_mem_addr);

#define	QAT_PHYS_CONTIG_ALLOC(pp_mem_addr, size_bytes)	\
  qat_mem_alloc_contig((void *)(pp_mem_addr), (size_bytes))
#define	QAT_PHYS_CONTIG_FREE(p_mem_addr)	\
  qat_mem_free_contig((void *)&(p_mem_addr))

/**********************************************************************/

/*
 * QAT compress/decompress procedure init
 */
extern int qat_dc_init(void);

/*
 * QAT compress/decompress procedure finish
 */
extern void qat_dc_fini(void);

/*
 * QAT crpty/checksum procedure init
 */
extern int qat_cy_init(void);

/*
 * QAT crpty/checksum procedure finish
 */
extern void qat_cy_fini(void);

/**
 * QAT hardware accelerator init
 *
 * @return VDO_SUCCESS or an error
 **/
extern int qat_init(void);

/**
 * QAT hardware accelerator finish
 **/
extern void qat_fini(void);

/**********************************************************************/

/**
 * Use QAT to compress block.
 *
 * @param dataKVIO  	The DataKVIO of source block
 * @param dir  		Compress or Decompress
 * @param src	 	The memory address of source block
 * @param src_len 	The length of source block
 * @param dst 		The memory address of destination block
 * @param dst_len	The length of destination block
 * @param c_len
 *
 * @return CPA_STATUS_SUCCESS or an error
 **/
extern int qat_compress(DataKVIO *dataKVIO, Cpa8U dir,
                        char *src, int src_len, char *dst,
			int dst_len, size_t *c_len);

/**********************************************************************/

/**
 * Use QAT to do checkcsum.
 *
 * @param dataKVIO  	The DataKVIO of source block
 * @param cksum  	The checksum algorithm
 * @param buf  		The memory address of source block
 * @param size	 	The size of source block
 * @param out 	A 256-bit checksum for cryptographic hashes
 *
 * @return CPA_STATUS_SUCCESS or an error
 **/
extern int qat_checksum(DataKVIO *dataKVIO, uint64_t cksum, uint8_t *src_buf,
		        uint64_t size, void *out);


#endif /* _SYS_QAT_H */
