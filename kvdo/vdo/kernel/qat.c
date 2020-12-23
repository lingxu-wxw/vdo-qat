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
#include "statusCodes.h"
#include <linux/slab.h>

/**********************************************************************/
CpaStatus qat_mem_alloc_contig(void **pp_mem_addr, Cpa32U size_bytes)
{
	*pp_mem_addr = kmalloc(size_bytes, GFP_KERNEL);
	if (*pp_mem_addr == NULL) {
		return (CPA_STATUS_RESOURCE);
	}
	return (CPA_STATUS_SUCCESS);
}

/**********************************************************************/
void qat_mem_free_contig(void **pp_mem_addr)
{
	if (*pp_mem_addr != NULL) {
		kfree(*pp_mem_addr);
		*pp_mem_addr = NULL;
	}
}

/**********************************************************************/
int qat_init(void)
{
	int ret;

	ret = qat_dc_init();
	if (ret != 0) {
		return (ret);
	}

	ret = qat_cy_init();
	if (ret != 0) {
		return (ret);
	}

	return VDO_SUCCESS;
}

/**********************************************************************/
void qat_fini(void)
{
	qat_dc_fini();
	qat_cy_fini();
}
