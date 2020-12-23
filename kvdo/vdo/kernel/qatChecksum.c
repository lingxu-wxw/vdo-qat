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

#include "qat.h"
#include "constants.h"
#include "dataKVIO.h"
#include "dataVIO.h"

/*
 * Max instances in a QAT device, each instance is a channel to submit jobs
 * to QAT hardware, this is only for pre-allocating instance and session
 * arrays; the actual number of instances are defined in the QAT driver's
 * configuration file.
 */
#define	QAT_CY_MAX_INSTANCES	48

/*
 * QAT Instance handle type
 * Handle used to uniquely identify an instance.
 */
static CpaInstanceHandle inst_handles[QAT_CY_MAX_INSTANCES];
static CpaCySymSessionCtx *cy_session_ctxs[QAT_CY_MAX_INSTANCES] = {0};

/*
 * QAT Device Instance
 */
static Cpa16U cy_num_inst = 0;
static Cpa32U inst_num = 0;

static boolean_t qat_cy_init_done = B_FALSE;

/**********************************************************************/
/*
 * Function Declare
 */
static void qat_cy_clean(void);

/**********************************************************************/
/*
 * The following variables are allocated on the stack because we block
 * until the callback comes back. If a non-blocking approach was to be
 * used then these variables should be dynamically allocated
*/
typedef struct cy_callback {
  DataKVIO  *dataKVIO;
  struct page *pages;
  CpaBufferList *buffer_list;
} cy_callback_t;

static void symSessionWaitForInflightReq(CpaCySymSessionCtx pSessionCtx)
{
/* Session reuse is available since Cryptographic API version 2.2 */
  CpaBoolean sessionInUse = CPA_FALSE;
  do
  {
    cpaCySymSessionInUse(pSessionCtx, &sessionInUse);
  } while (sessionInUse);

  return;
}

/**********************************************************************/
/*
 * Register callback in cpaCySymInitSession()
 */
static void symcallback(void *p_callback, CpaStatus status,
		        const CpaCySymOp operation,void *sym_op_data,
			CpaBufferList *buf_list_dst, CpaBoolean verify)
{
  cy_callback_t *callback = p_callback;
  DataKVIO  *dataKVIO = callback->dataKVIO;
  DataVIO  *dataVIO  = &dataKVIO->dataVIO;
  struct page *pages = callback->pages;
  CpaBufferList *buffer_list = callback->buffer_list;

  Cpa8U *chunk_buf = ((CpaCySymOpData *)sym_op_data)->pDigestResult;
  if (status != CPA_STATUS_SUCCESS)
    goto done;
  memmove((void *)&dataVIO->chunkName, chunk_buf, UDS_CHUNK_NAME_SIZE);

done:
  kunmap(pages);

  QAT_PHYS_CONTIG_FREE(chunk_buf);
  QAT_PHYS_CONTIG_FREE(sym_op_data);
  QAT_PHYS_CONTIG_FREE(buffer_list->pPrivateMetaData);
  QAT_PHYS_CONTIG_FREE(buffer_list->pBuffers);
  QAT_PHYS_CONTIG_FREE(buffer_list);
  QAT_PHYS_CONTIG_FREE(callback);
  dataKVIO->dedupeContext.chunkName = &dataVIO->chunkName;
  kvdoEnqueueDataVIOCallback(dataKVIO);
}

/**********************************************************************/
/*
 * Initial step is responsible for init device instances.
 */
int qat_cy_init(void)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaCySymSessionSetupData sd = { 0 };
  Cpa32U session_ctx_size;

  if (qat_cy_init_done) {
    return (0);
  }

  /* Get the number of device instances */
  status = cpaCyGetNumInstances(&cy_num_inst);
  if (status != CPA_STATUS_SUCCESS) {
    return (-1);
  }

  /* if the user has configured no QAT encryption units just return */
  if (cy_num_inst == 0) {
    return (0);
  }

  if (cy_num_inst > QAT_CY_MAX_INSTANCES) {
    cy_num_inst = QAT_CY_MAX_INSTANCES;
  }

  /* Get the device instances */
  status = cpaCyGetInstances(cy_num_inst, &inst_handles[0]);
  if (status != CPA_STATUS_SUCCESS) {
    return (-1);
  }

  sd.sessionPriority = CPA_CY_PRIORITY_NORMAL;
  sd.symOperation = CPA_CY_SYM_OP_HASH;
  sd.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA256;
  sd.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
  sd.hashSetupData.digestResultLenInBytes = UDS_CHUNK_NAME_SIZE;
  /* Place the digest result in a buffer unrelated to srcBuffer */
  sd.digestIsAppended = CPA_FALSE;
  /* Generate the digest */
  sd.verifyDigest = CPA_FALSE;


  for (Cpa16U i = 0; i < cy_num_inst; i++) {
    /* Set the virtual to physical address translation routine for the instance */
    status = cpaCySetAddressTranslation(inst_handles[i], (void *)virt_to_phys);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }
    /* Cryptographic Component Initialization and Start function */
    status = cpaCyStartInstance(inst_handles[i]);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }
    /* Get the size of the memory allocated to hold the session information */
    status = cpaCySymSessionCtxGetSize(inst_handles[i], &sd, &session_ctx_size);
    if (status != CPA_STATUS_SUCCESS) {
      return (status);
    }
    /* Allocate session context */
    status = QAT_PHYS_CONTIG_ALLOC(&cy_session_ctxs[i], session_ctx_size);
    if (status != CPA_STATUS_SUCCESS) {
      return (status);
    }
    status = cpaCySymInitSession(inst_handles[i], symcallback,
             &sd, cy_session_ctxs[i]);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }


  }

  qat_cy_init_done = B_TRUE;
  return (0);

done:
  qat_cy_clean();
  return (-1);
}

/**********************************************************************/
/*
 * Clean step is responsible for freeing allocated memory.
 */
void qat_cy_clean(void)
{
  for (Cpa16U i = 0; i < cy_num_inst; i++) {
    symSessionWaitForInflightReq(cy_session_ctxs[i]);
    cpaCySymRemoveSession(inst_handles[i], cy_session_ctxs[i]);
    /* Stop the Compression component and free all system resources associated with it */
    cpaCyStopInstance(inst_handles[i]);
    if (cy_session_ctxs[i] != NULL)
      QAT_PHYS_CONTIG_FREE(cy_session_ctxs[i]);
  }
  cy_num_inst = 0;
  qat_cy_init_done = B_FALSE;
}

/**********************************************************************/
/*
 * Final step is responsible for freeing allocated memory and sending the item
 * to deduplication processing module to redirect the successor module.
 */
void qat_cy_fini(void)
{
  if (!qat_cy_init_done) {
    return;
  }
  qat_cy_clean();
}

/**********************************************************************/
/*
 * Entry point for QAT accelerated cryptographic.
 */
int qat_checksum(DataKVIO *dataKVIO, uint64_t cksum, uint8_t *src_buf,
		 uint64_t size, void *out)
{
  CpaStatus status = CPA_STATUS_SUCCESS;

  CpaInstanceHandle inst_handle;
  CpaCySymSessionCtx *cy_session_ctx;
  Cpa8U *chunk_buf = NULL;
  CpaBufferList *buffer_list = NULL;
  CpaFlatBuffer *flat_src_buf = NULL;
  Cpa32U meta_size = 0;
  CpaCySymOpData *sym_op_data;
  struct page *pages;
  cy_callback_t *callback;
  Cpa16U nr_bufs = 1;

  /*
   * cy_inst_num is assigned in calling routine, in a way like round robin
   * with atomic operation, to load balance for hardware instances.
   */
  Cpa16U i;
  i = (Cpa32U)atomic_inc_return((atomic_t *)&inst_num) % cy_num_inst;
  inst_handle = inst_handles[i];
  cy_session_ctx = cy_session_ctxs[i];

  /* Init Buffer Lists and Allocate Memory */
  status = QAT_PHYS_CONTIG_ALLOC(&buffer_list, sizeof(CpaBufferList) +
                                 sizeof (CpaFlatBuffer));
  /* build source metadata buffer list */
  status = cpaCyBufferListGetMetaSize(inst_handle, nr_bufs, &meta_size);
  if (status != CPA_STATUS_SUCCESS) {
    return (status);
  }

  status = QAT_PHYS_CONTIG_ALLOC(&buffer_list->pPrivateMetaData, meta_size);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* Allocate Memory for source buffer array */
  status = QAT_PHYS_CONTIG_ALLOC(&flat_src_buf, nr_bufs * sizeof(CpaFlatBuffer));
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* Each block has a 256-bit checksum -- strong enough for cryptographic hashes. */
  status = QAT_PHYS_CONTIG_ALLOC(&chunk_buf, UDS_CHUNK_NAME_SIZE);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }
  QAT_PHYS_CONTIG_ALLOC(&callback, sizeof(cy_callback_t));
  QAT_PHYS_CONTIG_ALLOC(&sym_op_data, sizeof(CpaCySymOpData));

  pages = qat_mem_to_page(src_buf);
  flat_src_buf->pData = kmap(pages);
  flat_src_buf->dataLenInBytes = VDO_BLOCK_SIZE;

  /* Set Operation Data */
  sym_op_data->sessionCtx = cy_session_ctx;
  sym_op_data->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
  sym_op_data->pDigestResult = chunk_buf;
  sym_op_data->hashStartSrcOffsetInBytes = 0;
  sym_op_data->messageLenToHashInBytes = VDO_BLOCK_SIZE;

  /* Update source buffer list */
  buffer_list->numBuffers = 1;
  buffer_list->pBuffers = flat_src_buf;

  callback->dataKVIO = dataKVIO;
  callback->pages = pages;
  callback->buffer_list = buffer_list;
  /* Perform symmetric operation */
  do {
    status = cpaCySymPerformOp(
             inst_handle,
             callback, 		/* data sent as is to the callback function*/
             sym_op_data,		/* operational data struct */
             buffer_list,	/* source buffer list */
             buffer_list, 	/* same src & dst for an in-place operation*/
             NULL);
  } while (status == CPA_STATUS_RETRY);

  if (status == CPA_STATUS_SUCCESS) {
    return  status;
  }

done:
  kunmap(pages);
  /*
   * At this stage, the callback function should have returned,
   * so it is safe to free the memory
   */
  QAT_PHYS_CONTIG_FREE(chunk_buf);
  QAT_PHYS_CONTIG_FREE(callback);
  QAT_PHYS_CONTIG_FREE(sym_op_data);
  QAT_PHYS_CONTIG_FREE(flat_src_buf);
  QAT_PHYS_CONTIG_FREE(buffer_list->pPrivateMetaData);
  QAT_PHYS_CONTIG_FREE(buffer_list);

  return (status);
}

