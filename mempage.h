/*
 Copyright(c) 2017 Qizhiqiang
 All rights reserved.
 
 Redistribution and use in source and binary forms are permitted
 provided that the above copyright notice and this paragraph are
 duplicated in all such forms and that any documentation,
 advertising materials, and other materials related to such
 distribution and use acknowledge that the software was developed
 by Qizhiqiang.The name may not be used to endorse or promote
 products derived from this software without specific prior
 written permission.
 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef mempage_h
#define mempage_h

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
#define MEMPAGE_API extern "C"
#else
#define MEMPAGE_API
#endif

#define MEMPAGE_SIZE_BIT 12

typedef uint32_t mempage_int_t;
typedef struct mempage_s* mempage_t;

enum {
	MEMPAGE_OK = 0,
	MEMPAGE_BUFFER_SIZE_ERR,
	MEMPAGE_SEGMENT_ERR,
};

MEMPAGE_API mempage_t mempage_new(const char* data, mempage_int_t data_size);
MEMPAGE_API mempage_t mempage_from_file(const char* filename);
MEMPAGE_API mempage_int_t mempage_size(mempage_t mp);
MEMPAGE_API void mempage_delete(mempage_t mp);
MEMPAGE_API int mempage_extract(mempage_t mp, mempage_int_t offset, mempage_int_t size, char* out);
MEMPAGE_API int mempage_write(const char* data, mempage_int_t data_size, const char* filename);

#endif /* mempage_h */
