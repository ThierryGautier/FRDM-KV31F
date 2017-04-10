/*
 * Copyright (c) 2013 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*******************************************************************************
*
* @brief Main GDFLIB header file for devices with FPU. 
* 
*******************************************************************************/
#ifndef _GDFLIB_FP_H_
#define _GDFLIB_FP_H_

#if defined(__cplusplus)
extern "C" {
#endif
  
/*******************************************************************************
* Includes
*******************************************************************************/
#include "gdflib.h"
#include "GDFLIB_FilterIIR1_FLT.h"
#include "GDFLIB_FilterIIR2_FLT.h"
#include "GDFLIB_FilterIIR3_FLT.h"
#include "GDFLIB_FilterIIR4_FLT.h"    
#include "GDFLIB_FilterMA_FLT.h"      

/*******************************************************************************
* Macros 
*******************************************************************************/
#define GDFLIB_FilterIIR1Init_FLT(psParam)                                     \
        GDFLIB_FilterIIR1Init_FLT_Ci(psParam)
#define GDFLIB_FilterIIR1_FLT(f16InX, psParam)                                 \
        GDFLIB_FilterIIR1_FLT_Ci(f16InX, psParam)                              
#define GDFLIB_FilterIIR2Init_FLT(psParam)                                     \
        GDFLIB_FilterIIR2Init_FLT_C(psParam)                                  
#define GDFLIB_FilterIIR2_FLT(f16InX, psParam)                                 \
        GDFLIB_FilterIIR2_FLT_C(f16InX, psParam) 
#define GDFLIB_FilterIIR3Init_FLT(psParam)                                     \
        GDFLIB_FilterIIR3Init_FLT_C(psParam)                                  
#define GDFLIB_FilterIIR3_FLT(f16InX, psParam)                                 \
        GDFLIB_FilterIIR3_FLT_C(f16InX, psParam)                              
#define GDFLIB_FilterIIR4Init_FLT(psParam)                                     \
        GDFLIB_FilterIIR4Init_FLT_C(psParam)                                  
#define GDFLIB_FilterIIR4_FLT(f16InX, psParam)                                 \
        GDFLIB_FilterIIR4_FLT_C(f16InX, psParam)             
#define GDFLIB_FilterMAInit_FLT(f16InitVal, psParam)                           \
        GDFLIB_FilterMAInit_FLT_Ci(f16InitVal, psParam)                        
#define GDFLIB_FilterMA_FLT(f16InX, psParam)                                   \
        GDFLIB_FilterMA_FLT_Ci(f16InX, psParam)                 
            
#if defined(__cplusplus)
}
#endif

#endif /* _GDFLIB_FP_H_ */