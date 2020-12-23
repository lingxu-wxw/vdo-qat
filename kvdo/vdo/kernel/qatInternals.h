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

#ifndef _QAT_INTERNALS_H
#define _QAT_INTERNALS_H

#include "cpa.h"
#include "dc/cpa_dc.h"

typedef enum qat_compress_dir {
  QAT_DECOMPRESS = 0,
  QAT_COMPRESS = 1,
} qat_compress_dir_t;

typedef enum {
  B_FALSE = 0,
  B_TRUE = 1,
} boolean_t;

#endif /* _QAT_INTERNALS_H  */
