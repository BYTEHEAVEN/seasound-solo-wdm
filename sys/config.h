// $Id: config.h,v 1.4 2001/08/23 21:55:31 tekhedd Exp $
// Copyright (C) 2001 Tom Surace. All rights reserved.

#ifndef _CONFIG_H_
#define _CONFIG_H_

// sample size in bytes (32 bits, 24 bits significant, 20 bits useful
// 18 bits really necessary for pro audio --trs)
#define SSS_SAMPLE_BYTES (4)   

#define SSS_PAGE_FRAMES_MIN          (0x0004)
#define SSS_PAGE_FRAMES_MAX          (0x0400)

#define SSS_PLAY_FRAME_SIZE          (10) // samples per frame 
#define SSS_REC_FRAME_SIZE           (12) // samples per frame

#define SSS_PLAY_FRAME_BYTES (SSS_PLAY_FRAME_SIZE * SSS_SAMPLE_BYTES)
#define SSS_REC_FRAME_BYTES  (SSS_REC_FRAME_SIZE * SSS_SAMPLE_BYTES)

// We can use more pages, but only if they are at least 
// SSS_PAGE_FRAMES_MIN in size. Start from MAX and work your
// way down...
#define SSS_DMA_PAGE_COUNT_MIN (2)
#define SSS_DMA_PAGE_COUNT_MAX (8)

// Total size of buffers allocated for play/record, in bytes.
// These must be a multiple of DMA_pageDwords, and it must
// also be a multiple of 10.
//#define SSS_PLAY_BUFFER_SIZE(page_dwords)  \
//    (page_dwords * SSS_SAMPLE_BYTES * SSS_PLAY_FRAME_SIZE * SSS_DMA_PAGE_COUNT)
//   
//#define SSS_REC_BUFFER_SIZE(page_dwords)   \
//    (page_dwords * SSS_SAMPLE_BYTES * SSS_REC_FRAME_SIZE * SSS_DMA_PAGE_COUNT)



#endif // _CONFIG_H_
