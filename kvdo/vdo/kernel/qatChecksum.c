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

#define	MAX_PAGE_NUM			1024

/*
 * QAT Instance handle type
 * Handle used to uniquely identify an instance.
 */
static CpaInstanceHandle inst_handles[QAT_CY_MAX_INSTANCES];

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
	CpaBoolean verify_result;
	struct completion complete;
} cy_callback_t;

/**********************************************************************/
/*
 * Register callback in cpaCySymInitSession()
 */
static void symcallback(void *p_callback, CpaStatus status, const CpaCySymOp operation,
    void *sym_op_data, CpaBufferList *buf_list_dst, CpaBoolean verify)
{
	cy_callback_t *callback = p_callback;

	if (callback != NULL) {
		/* indicate that the function has been called */
		callback->verify_result = verify;
		complete(&callback->complete);
	}
}

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
 * Initial step is responsible for init device instances.
 */
int qat_cy_init(void)
{
	CpaStatus status = CPA_STATUS_SUCCESS;

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
    /* Stop the Compression component and free all system resources associated with it */
		cpaCyStopInstance(inst_handles[i]);
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
 * Init Checksum session.
 */
static CpaStatus qat_init_checksum_session_ctx(CpaInstanceHandle inst_handle,
    CpaCySymSessionCtx **cy_session_ctx, Cpa64U cksum)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U session_ctx_size;
	Cpa32U hash_algorithm;
	CpaCySymSessionSetupData sd = { 0 };

	/*
	 * SHA512/256 is a different IV from standard SHA512. QAT does not 
	 * support SHA512/256, so we can only support SHA256.
	 */
	if (cksum == VIO_CHECKSUM_SHA256) {
		hash_algorithm = CPA_CY_SYM_HASH_SHA256;
    } else {
		return (CPA_STATUS_FAIL);
    }

	/*
     * We now populate the fields of the session operational data and create
     * the session.  Note that the size required to store a session is
     * implementation-dependent, so we query the API first to determine how
     * much memory to allocate, and then allocate that memory.
     */
	/* populate symmetric session data structure for a plain hash operation */
	sd.sessionPriority = CPA_CY_PRIORITY_NORMAL;
	sd.symOperation = CPA_CY_SYM_OP_HASH;
	sd.hashSetupData.hashAlgorithm = hash_algorithm;
	sd.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
	sd.hashSetupData.digestResultLenInBytes = UDS_CHUNK_NAME_SIZE;
	/* Place the digest result in a buffer unrelated to srcBuffer */
	sd.digestIsAppended = CPA_FALSE;
	/* Generate the digest */
	sd.verifyDigest = CPA_FALSE;

    /* Get the size of the memory allocated to hold the session information */
	status = cpaCySymSessionCtxGetSize(inst_handle, &sd, &session_ctx_size);
	if (status != CPA_STATUS_SUCCESS) {
		return (status);
    }

    /* Allocate session context */
	status = QAT_PHYS_CONTIG_ALLOC(cy_session_ctx, session_ctx_size);
	if (status != CPA_STATUS_SUCCESS) {
		return (status);
    }

    /* Initialize the Hash session */
	status = cpaCySymInitSession(inst_handle, symcallback, 
                              &sd, *cy_session_ctx);
	if (status != CPA_STATUS_SUCCESS) {
		QAT_PHYS_CONTIG_FREE(*cy_session_ctx);
		return (status);
	}

	return (CPA_STATUS_SUCCESS);
}

/**********************************************************************/
/*
 * Init cryptographic buffer lists.
 */
static CpaStatus qat_init_cy_buffer_lists(CpaInstanceHandle inst_handle, 
    uint32_t nr_bufs, CpaBufferList *src, CpaBufferList *dst)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa32U meta_size = 0;

    /* build source metadata buffer list */
	status = cpaCyBufferListGetMetaSize(inst_handle, nr_bufs, &meta_size);
	if (status != CPA_STATUS_SUCCESS) {
		return (status);
    }

	status = QAT_PHYS_CONTIG_ALLOC(&src->pPrivateMetaData, meta_size);
	if (status != CPA_STATUS_SUCCESS) {
		goto done;
    }

	if (src != dst) {
		status = QAT_PHYS_CONTIG_ALLOC(&dst->pPrivateMetaData, meta_size);
		if (status != CPA_STATUS_SUCCESS) {
			goto done;
        }
	}

	return (CPA_STATUS_SUCCESS);

done:

	QAT_PHYS_CONTIG_FREE(src->pPrivateMetaData);

	if (src != dst) {
		QAT_PHYS_CONTIG_FREE(dst->pPrivateMetaData);
    }

	return (status);
}

/**********************************************************************/
/*
 * Entry point for QAT accelerated cryptographic.
 */
int qat_checksum(uint64_t cksum, uint8_t *src_buf, uint64_t size, void *out)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

	CpaInstanceHandle inst_handle;
	Cpa8U *dst_buf = NULL;
	CpaBufferList src_buffer_list = { 0 };
	CpaFlatBuffer *flat_src_buf_array = NULL;
	CpaFlatBuffer *flat_src_buf = NULL;

    CpaCySymSessionCtx *cy_session_ctx = NULL;
    CpaCySymOpData sym_op_data = { 0 };

	struct page *pages_in[MAX_PAGE_NUM];
	Cpa32U page_num = 0;
	Cpa32U page_off = 0;

    Cpa32U left_bytes = 0;
	Cpa8S *data_block = NULL;
	cy_callback_t callback;

    /* 
	 * We increment num_bufs by 2 to allow us to handle non page-aligned 
     * buffer addresses and buffers whose sizes are not divisible by 
     * PAGE_SIZE. 
	 */
    Cpa16U nr_bufs = (size >> PAGE_SHIFT) + 2;

    /* 
	 * cy_inst_num is assigned in calling routine, in a way like round robin 
	 * with atomic operation, to load balance for hardware instances. 
	 */
    Cpa16U i;
	i = (Cpa32U)atomic_inc_return((atomic_t *)&inst_num) % cy_num_inst;
	inst_handle = inst_handles[i];

	status = qat_init_checksum_session_ctx(inst_handle, &cy_session_ctx, cksum);
	if (status != CPA_STATUS_SUCCESS) {
		return (status);
	}

	/* Init Buffer Lists and Allocate Memory */
	status = qat_init_cy_buffer_lists(inst_handle, nr_bufs, &src_buffer_list, &src_buffer_list);
	if (status != CPA_STATUS_SUCCESS) {
		goto done;
    }

	/* Allocate Memory for source buffer array */
	status = QAT_PHYS_CONTIG_ALLOC(&flat_src_buf_array, nr_bufs * sizeof(CpaFlatBuffer));
	if (status != CPA_STATUS_SUCCESS) {
		goto done;
    }

	/* Each block has a 256-bit checksum -- strong enough for cryptographic hashes. */
	status = QAT_PHYS_CONTIG_ALLOC(&dst_buf, UDS_CHUNK_NAME_SIZE);
	if (status != CPA_STATUS_SUCCESS) {
		goto done;
    }

	/* Tranverse the data block by pages */
	flat_src_buf = flat_src_buf_array;
	data_block = src_buf;
	left_bytes = size;

	while (left_bytes > 0) {
		/* Get current data block */
		page_off = ((long)data_block & ~PAGE_MASK);
		pages_in[page_num] = qat_mem_to_page(data_block);

		flat_src_buf->pData = kmap(pages_in[page_num]) + page_off;
		flat_src_buf->dataLenInBytes = min((long)PAGE_SIZE - page_off, (long)left_bytes);

		/* Update current data block and left bytes */
		data_block += flat_src_buf->dataLenInBytes;
		left_bytes -= flat_src_buf->dataLenInBytes;
		
		flat_src_buf += 1;
		page_num += 1;
	}

	/* Update Operation Data */
	sym_op_data.sessionCtx = cy_session_ctx;
	sym_op_data.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
	sym_op_data.pDigestResult = dst_buf;
	sym_op_data.hashStartSrcOffsetInBytes = 0;
	sym_op_data.messageLenToHashInBytes = size;

	/* Update source buffer list */
	src_buffer_list.numBuffers = page_num;
	src_buffer_list.pBuffers = flat_src_buf_array;

	/* Update cryptographic callback */
	callback.verify_result = CPA_FALSE;

	/* 
	* initialisation for callback; the "complete" variable is used by the
	* callback function to indicate it has been called
	*/
	init_completion(&callback.complete);

	/* Perform symmetric operation */
	do {
		status = cpaCySymPerformOp(
			inst_handle, 
			&callback, 		/* data sent as is to the callback function*/
			&sym_op_data,		/* operational data struct */
			&src_buffer_list,	/* source buffer list */
			&src_buffer_list, 	/* same src & dst for an in-place operation*/
			NULL);
	} while (status == CPA_STATUS_RETRY);

	if (status != CPA_STATUS_SUCCESS) {
		goto done;
	}

	/* we now wait until the completion of the operation. */
	wait_for_completion(&callback.complete);

	if (callback.verify_result == CPA_FALSE) {
		status = CPA_STATUS_FAIL;
		goto done;
	}

    memmove(out, dst_buf, UDS_CHUNK_NAME_SIZE);

done:

	for (i = 0; i < page_num; i++) {
		kunmap(pages_in[i]);
    }

	/* Wait for inflight requests before removing session */
	symSessionWaitForInflightReq(cy_session_ctx);

    /* Remove the session - session init has already succeeded */
	cpaCySymRemoveSession(inst_handle, cy_session_ctx);
	
	/* 
	 * At this stage, the callback function should have returned,
     * so it is safe to free the memory 
	 */
    QAT_PHYS_CONTIG_FREE(dst_buf);
	QAT_PHYS_CONTIG_FREE(src_buffer_list.pPrivateMetaData);
	QAT_PHYS_CONTIG_FREE(cy_session_ctx);
	QAT_PHYS_CONTIG_FREE(flat_src_buf_array);

	return (status);
}

