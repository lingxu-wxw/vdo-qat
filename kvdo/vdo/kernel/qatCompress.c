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
#define	QAT_DC_MAX_INSTANCES	48

/*
 * ZLIB head and foot size
 */
#define	ZLIB_HEAD_SZ		2
#define	ZLIB_FOOT_SZ		4

/*
 * QAT Instance handle type
 * Handle used to uniquely identify an instance.
 */
static CpaInstanceHandle inst_handles[QAT_DC_MAX_INSTANCES];

/*
 * QAT Compression API session handle type
 * Handle used to uniquely identify a Compression API session handle.
 */
static CpaDcSessionHandle dc_session_handles[QAT_DC_MAX_INSTANCES];

/*
 * Scatter/Gather buffer list containing an array of flat buffers.
 */
static CpaBufferList **buffer_array[QAT_DC_MAX_INSTANCES];

/*
 * QAT Device Instance
 */
static Cpa16U dc_num_inst = 0;
static Cpa32U inst_num = 0;

static boolean_t qat_dc_init_done = B_FALSE;

/**********************************************************************/
/*
 * Function Declare
 */
static void qat_dc_clean(void);
static void dc_callback(void *p_callback, CpaStatus status);

typedef struct dc_callback {
  DataKVIO *dataKVIO;
  CpaDcSessionHandle dc_session_handle;
  CpaInstanceHandle inst_handle;
  Cpa8U dir;
  CpaDcRqResults dc_results;
  Cpa8U *buffer_meta_src;
  Cpa8U *buffer_meta_dst;
  CpaBufferList *buffer_list_src;
  CpaBufferList *buffer_list_dst;
} dc_callback_t;

/**********************************************************************/
/*
 * Initial step is responsible for init device instances, init allocated memory
 * and register callback function dc_callback().
 */
int qat_dc_init(void)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  Cpa32U session_size = 0;
  Cpa32U ctx_size = 0;
  Cpa16U num_inter_buffer_lists = 0;
  Cpa16U buffer_num = 0;
  Cpa32U buffer_meta_size = 0;
  CpaDcSessionSetupData sd = {0};

  if (qat_dc_init_done) {
    return (0);
  }

  /* Get the number of device instances */
  status = cpaDcGetNumInstances(&dc_num_inst);
  if (status != CPA_STATUS_SUCCESS) {
    return (-1);
  }

  /* if the user has configured no QAT compression units just return */
  if (dc_num_inst == 0) {
    return (0);
  }

  if (dc_num_inst > QAT_DC_MAX_INSTANCES) {
    dc_num_inst = QAT_DC_MAX_INSTANCES;
  }

  /* Get the device instances */
  status = cpaDcGetInstances(dc_num_inst, &inst_handles[0]);
  if (status != CPA_STATUS_SUCCESS) {
    return (-1);
  }

  for (Cpa16U i = 0; i < dc_num_inst; i++) {
    /* Set the virtual to physical address translation routine for the instance */
    cpaDcSetAddressTranslation(inst_handles[i], (void*)virt_to_phys);

    status = cpaDcBufferListGetMetaSize(inst_handles[i], 1,
                                        &buffer_meta_size);

    /* Determine the number of intermediate buffer lists
     * required by compression instance
     */
    if (status == CPA_STATUS_SUCCESS) {
      status = cpaDcGetNumIntermediateBuffers(inst_handles[i],
               &num_inter_buffer_lists);
    }

    if (status == CPA_STATUS_SUCCESS && num_inter_buffer_lists != 0) {
      status = QAT_PHYS_CONTIG_ALLOC(&buffer_array[i],
               num_inter_buffer_lists * sizeof (CpaBufferList *));
    }

    /* Init CpaFlatBuffer */
    for (buffer_num = 0; buffer_num < num_inter_buffer_lists; buffer_num++) {
      if (status == CPA_STATUS_SUCCESS) {
        status = QAT_PHYS_CONTIG_ALLOC(
               &buffer_array[i][buffer_num], sizeof (CpaBufferList));
      }

      if (status == CPA_STATUS_SUCCESS) {
        status = QAT_PHYS_CONTIG_ALLOC(
                 &buffer_array[i][buffer_num]->pPrivateMetaData, buffer_meta_size);
      }

      if (status == CPA_STATUS_SUCCESS) {
        status = QAT_PHYS_CONTIG_ALLOC(
                 &buffer_array[i][buffer_num]->pBuffers, sizeof (CpaFlatBuffer));
      }

      /*
       *  implementation requires an intermediate buffer approximately
       *  twice the size of output buffer, which is 2x max buffer size here.
       */
      if (status == CPA_STATUS_SUCCESS) {
        status = QAT_PHYS_CONTIG_ALLOC(
                 &buffer_array[i][buffer_num]->pBuffers->pData, 2 * QAT_MAX_BUF_SIZE);
        if (status != CPA_STATUS_SUCCESS) {
          goto done;
        }

        buffer_array[i][buffer_num]->numBuffers = 1;
        buffer_array[i][buffer_num]->pBuffers->dataLenInBytes = 2 * QAT_MAX_BUF_SIZE;
      }
    }

    /* Compression Component Initialization and Start function */
    status = cpaDcStartInstance(inst_handles[i],
                                num_inter_buffer_lists,
                                buffer_array[i]);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }

    /* Complete the information in CpaDcSessionSetupData to setup a session */
    sd.compLevel = CPA_DC_L1;
    sd.compType = CPA_DC_DEFLATE;
    sd.huffType = CPA_DC_HT_FULL_DYNAMIC;
    sd.sessDirection = CPA_DC_DIR_COMBINED;
    sd.sessState = CPA_DC_STATELESS;
    sd.deflateWindowSize = 7;
    sd.checksum = CPA_DC_ADLER32;

    /* Get the size of the memory required to hold the session information */
    status = cpaDcGetSessionSize(inst_handles[i], &sd,
                                 &session_size, &ctx_size);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }

    QAT_PHYS_CONTIG_ALLOC(&dc_session_handles[i], session_size);
    if (dc_session_handles[i] == NULL) {
        goto done;
    }

    /*
     * Initialize compression or decompression session
     * Register callback function dc_callback()
     */
    status = cpaDcInitSession(inst_handles[i], dc_session_handles[i],
                             &sd, NULL, dc_callback);
    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }
  }

  qat_dc_init_done = B_TRUE;
  return (0);

done:

  qat_dc_clean();
  return (-1);
}

/**********************************************************************/
/*
 * Clean step is responsible for freeing allocated memory.
 */
static void qat_dc_clean(void)
{
  Cpa16U buffer_num = 0;
  Cpa16U num_inter_buffer_lists = 0;

  for (Cpa16U i = 0; i < dc_num_inst; i++) {
    /* Stop the Compression component and free all system resources associated with it */
    cpaDcStopInstance(inst_handles[i]);
    QAT_PHYS_CONTIG_FREE(dc_session_handles[i]);

    if (buffer_array[i] != NULL) {
      /*
       * Determine the number of intermediate buffer lists required by
       * a compression instance.
       */
      cpaDcGetNumIntermediateBuffers(inst_handles[i],
                                     &num_inter_buffer_lists);

      /* free intermediate buffers  */
      for (buffer_num = 0; buffer_num < num_inter_buffer_lists; buffer_num++) {
        CpaBufferList *buffer_inter = buffer_array[i][buffer_num];
        if (buffer_inter->pBuffers) {
          QAT_PHYS_CONTIG_FREE(buffer_inter->pBuffers->pData);
          QAT_PHYS_CONTIG_FREE(buffer_inter->pBuffers);
        }

        QAT_PHYS_CONTIG_FREE(buffer_inter->pPrivateMetaData);
        QAT_PHYS_CONTIG_FREE(buffer_inter);
      }
    }
  }

  dc_num_inst = 0;
  qat_dc_init_done = B_FALSE;
}

/**********************************************************************/
/*
 * Final step is responsible for freeing allocated memory and sending the item
 * to compression processing module to redirect the successor module.
 */
void qat_dc_fini(void)
{
  if (!qat_dc_init_done) {
    return;
  }

  qat_dc_clean();
}

/**********************************************************************/
/*
 * Callback function using for QAT accelerator is registered in initialization step,
 * and will be invoked when hardware accelerator completes the task.
 * It has only two parameters, one is the pointer to callback parameter p_callback,
 * another is status indicates whether compression during hardware processing is successful.
 */
static void dc_callback(void *cb, CpaStatus status)
{
  dc_callback_t *callback = (dc_callback_t *)cb;
  DataKVIO *dataKVIO = callback->dataKVIO;
  CpaDcSessionHandle dc_session_handle = callback->dc_session_handle;
  Cpa32U compressed_sz;
  CpaBufferList *buffer_list_dst = callback->buffer_list_dst;
  CpaFlatBuffer *flat_buffer_dst = NULL;
  CpaDcRqResults *dc_results = &callback->dc_results;

  DataVIO *dataVIO = &dataKVIO->dataVIO;
  ReadBlock *readBlock = &dataKVIO->readBlock;

  if (callback->dir == QAT_COMPRESS) {
    if (status != CPA_STATUS_SUCCESS || dc_results->status != CPA_DC_OK) {
      dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
      goto done;
    }

    /* In write workflow, the length of produced compressed result will be checked
     * if there is enough remaining space for footer size. If no enough footer size,
     * data will be marked as incompressible data similar to calling routine
     * and successor procedure will be jumped and go directly to the finial step.
     */
    compressed_sz = dc_results->produced;
    if (compressed_sz + ZLIB_HEAD_SZ + ZLIB_FOOT_SZ > VDO_BLOCK_SIZE) {
      dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
      goto done;
    }

    if (((compressed_sz + ZLIB_HEAD_SZ) % PAGE_SIZE)
          + ZLIB_FOOT_SZ > PAGE_SIZE) {
      dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
      goto done;
    }

    /* Construct CpaFlatbuffer */
    flat_buffer_dst = (CpaFlatBuffer *)(buffer_list_dst + 1);

    flat_buffer_dst->pData = (char*)((unsigned long)flat_buffer_dst->pData +
		             (compressed_sz));
    flat_buffer_dst->dataLenInBytes = ZLIB_FOOT_SZ;

    dc_results->produced = 0;
    status = cpaDcGenerateFooter(dc_session_handle,
                                 flat_buffer_dst, dc_results);
    if (status != CPA_STATUS_SUCCESS) {
      dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
      goto done;
    }

    /* Calculate the length of destination block */
    Cpa32U destLen = compressed_sz + dc_results->produced + ZLIB_HEAD_SZ;

    if (status == CPA_STATUS_SUCCESS && destLen <= VDO_BLOCK_SIZE) {
      /* The scratch block will be used to contain the compressed data. */
      dataVIO->compression.data = dataKVIO->scratchBlock;
      dataVIO->compression.size = destLen;
     } else {
       /* Use block size plus one as an indicator for uncompressible data. */
       dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
       goto done;
     }
  } else {
    if (status != CPA_STATUS_SUCCESS || dc_results->status != CPA_DC_OK) {
      /* In read workflow, if status is not success,
       * the status field of readBlock will be marked as invalid fragment
       * that indicated a read failure has occurred.
       */
      readBlock->status = VDO_INVALID_FRAGMENT;
    } else {
      /* For decompression, input data is a compressed fragment,
       * a piece of data saved in dataBlocks,
       * and result data are saved as scratchBlocks.
       */
      readBlock->data = dataKVIO->scratchBlock;
    }
  }

done:

  QAT_PHYS_CONTIG_FREE(callback->buffer_meta_src);
  QAT_PHYS_CONTIG_FREE(callback->buffer_meta_dst);
  QAT_PHYS_CONTIG_FREE(callback->buffer_list_src);
  QAT_PHYS_CONTIG_FREE(callback->buffer_list_dst);

  if (callback->dir == QAT_COMPRESS) {
    kvdoEnqueueDataVIOCallback(dataKVIO);
  } else {
    ReadBlock *readBlock = &dataKVIO->readBlock;
    readBlock->callback(dataKVIO);
  }
  QAT_PHYS_CONTIG_FREE(callback);
}

int qat_compress(DataKVIO *dataKVIO, Cpa8U dir, char *src, int src_len,
    char *dst, int dst_len, size_t *c_len)
{
  CpaStatus status = CPA_STATUS_SUCCESS;
  CpaInstanceHandle inst_handle;
  CpaDcSessionHandle dc_session_handle;
  CpaBufferList *buffer_list_src = NULL;
  CpaBufferList *buffer_list_dst = NULL;
  CpaFlatBuffer *flat_buffer_src = NULL;
  CpaFlatBuffer *flat_buffer_dst = NULL;
  Cpa8U *buffer_meta_src = NULL;
  Cpa8U *buffer_meta_dst = NULL;
  Cpa32U buffer_meta_size = 0;
  dc_callback_t *callback = NULL;
  CpaDcRqResults *dc_results = NULL;
  Cpa32U hdr_sz = 0;

  Cpa32U num_src_buf = 1;
  Cpa32U num_dst_buf = 1;

  /* calculate the size of buffer list */
  Cpa32U src_buffer_list_mem_size = sizeof (CpaBufferList) +
    (num_src_buf * sizeof (CpaFlatBuffer));
  Cpa32U dst_buffer_list_mem_size = sizeof (CpaBufferList) +
    (num_dst_buf * sizeof (CpaFlatBuffer));

  /*
   * dc_inst_num is assigned in calling routine, in a way like round robin
   * with atomic operation, to load balance for hardware instances.
   */
  Cpa16U i;
  i = (Cpa32U)atomic_inc_return((atomic_t *)&inst_num) % dc_num_inst;
  inst_handle = inst_handles[i];
  dc_session_handle = dc_session_handles[i];

  /* build source metadata buffer list */
  cpaDcBufferListGetMetaSize(inst_handle, num_src_buf,
                             &buffer_meta_size);

  status = QAT_PHYS_CONTIG_ALLOC(&buffer_meta_src, buffer_meta_size);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* build destination metadata buffer list */
  cpaDcBufferListGetMetaSize(inst_handle, num_dst_buf,
                             &buffer_meta_size);

  status = QAT_PHYS_CONTIG_ALLOC(&buffer_meta_dst, buffer_meta_size);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* build source buffer list */
  status = QAT_PHYS_CONTIG_ALLOC(&buffer_list_src, src_buffer_list_mem_size);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* always point to first one */
  flat_buffer_src = (CpaFlatBuffer *)(buffer_list_src + 1);
  buffer_list_src->pBuffers = flat_buffer_src;

  /* build destination buffer list */
  status = QAT_PHYS_CONTIG_ALLOC(&buffer_list_dst, dst_buffer_list_mem_size);
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  /* always point to first one */
  flat_buffer_dst = (CpaFlatBuffer *)(buffer_list_dst + 1);
  buffer_list_dst->pBuffers = flat_buffer_dst;

  buffer_list_src->numBuffers = 1;
  buffer_list_src->pPrivateMetaData = buffer_meta_src;
  flat_buffer_src->pData = src;
  flat_buffer_src->dataLenInBytes = src_len;

  buffer_list_dst->numBuffers = 1;
  buffer_list_dst->pPrivateMetaData = buffer_meta_dst;
  flat_buffer_dst->pData = dst;
  flat_buffer_dst->dataLenInBytes = dst_len;

  status = QAT_PHYS_CONTIG_ALLOC(&callback, sizeof(dc_callback_t));
  if (status != CPA_STATUS_SUCCESS) {
    goto done;
  }

  callback->buffer_meta_src = buffer_meta_src;
  callback->buffer_meta_dst = buffer_meta_dst;
  callback->buffer_list_src = buffer_list_src;
  callback->buffer_list_dst = buffer_list_dst;
  callback->dataKVIO = dataKVIO;
  callback->dc_session_handle = dc_session_handle;
  callback->dir = dir;
  dc_results = &callback->dc_results;

  if (dir == QAT_COMPRESS) {
    /*
     * As for compression, a header API is called to generate zlib style header.
     * The result will be put in the head of output buffer
     * with length of zlib header, which is fixed at 2 byte.
     */
    cpaDcGenerateHeader(dc_session_handle, buffer_list_dst->pBuffers, &hdr_sz);
    buffer_list_dst->pBuffers->pData += hdr_sz;
    buffer_list_dst->pBuffers->dataLenInBytes -= hdr_sz;

    /*
     * After data and context preparation, QAT kernel API for compression is called,
     * with a parameter as DataKVIO that preserving the callback in it.
     */
    status = cpaDcCompressData(inst_handle, dc_session_handle,
                               buffer_list_src, buffer_list_dst,
                               dc_results, CPA_DC_FLUSH_FINAL,
                               callback);

    /*
     * Once data is successfully sent to QAT accelerator,
     * the calling routine will finish this sending work and return the control back
     * to compression processing module, which will be ready to access next work item
     * from CPU queue and start a new sending work.
     */
    if (status != CPA_STATUS_SUCCESS) {
        goto done;
    }
  } else {
    /*
     * In decompression scenario, this header will be jumped,
     * leading hardware accelerator to start from content data.
     */
    buffer_list_src->pBuffers->pData += ZLIB_HEAD_SZ;
    buffer_list_src->pBuffers->dataLenInBytes -= ZLIB_HEAD_SZ;

    status = cpaDcDecompressData(inst_handle, dc_session_handle,
                                 buffer_list_src, buffer_list_dst,
                                 dc_results, CPA_DC_FLUSH_FINAL,
                                 callback);

    if (status != CPA_STATUS_SUCCESS) {
      goto done;
    }
  }

  return status;

done:
  QAT_PHYS_CONTIG_FREE(buffer_meta_src);
  QAT_PHYS_CONTIG_FREE(buffer_meta_dst);
  QAT_PHYS_CONTIG_FREE(buffer_list_src);
  QAT_PHYS_CONTIG_FREE(buffer_list_dst);
  QAT_PHYS_CONTIG_FREE(callback);

  if (dir == QAT_COMPRESS) {
    /*
     * If errors are occurred when sending request to hardware accelerator,
     * this request will be interrupted and marked as incompressible data,
     * and allocated memory will be freed before leaving the calling routine.
     */
    DataVIO *dataVIO = &dataKVIO->dataVIO;
    dataVIO->compression.size = VDO_BLOCK_SIZE + 1;
    kvdoEnqueueDataVIOCallback(dataKVIO);
  } else {
    /*
     * In read workflow, if status is not success,
     * the status field of readBlock will be marked as invalid fragment
     * that indicated a read failure has occurred.
     */
    ReadBlock *readBlock = &dataKVIO->readBlock;
    readBlock->status = VDO_INVALID_FRAGMENT;
    readBlock->callback(dataKVIO);
  }

  return (status);
}
