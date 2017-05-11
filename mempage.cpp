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

#include <stdlib.h>

#include <list>

#include "zlib/zlib.h"

#include "mempage.h"

#define MP_SEGMENT_SIZE (1 << MEMPAGE_SIZE_BIT)
#define MP_SEGMENT_SIZE_MOD(x) ((x) & ((1<<MEMPAGE_SIZE_BIT) - 1))
#define MP_SEGMENT_SIZE_FILL(x) ((x) + (PAGE_SIZE - 1)& ~(PAGE_SIZE - 1))
#define MP_SEGMENT_INDEX(x) ((x)>>MEMPAGE_SIZE_BIT)
#define MP_SEGMENT_SIZE_FLOOR(x) (((x)>>MEMPAGE_SIZE_BIT)<<MEMPAGE_SIZE_BIT)
#define MP_SEGMENT_TOTAL(x) (((x) + ((1 << MEMPAGE_SIZE_BIT) - 1))>>MEMPAGE_SIZE_BIT)

#define MP_SEGMENT_BUF_COUNT 16

struct mempage_s {
	mempage_int_t segment_count;
	mempage_int_t data_size;
	mempage_int_t* segment_sizes;
	mempage_int_t* segment_offsets;
	char* data;
};

typedef struct segment_buffer_s {
	char* buffer;
	mempage_t mp;
	mempage_int_t index;
	mempage_int_t data_size;
}*segment_buffer_t;

static segment_buffer_t extract_segment(mempage_t mp, mempage_int_t index) {
	typedef std::list<segment_buffer_t> segment_buffer_list_t;

	static segment_buffer_list_t segments_list;
	static segment_buffer_t segments_ptr;
	static char* segment_buffer_data;

	if (!segment_buffer_data) {
		segment_buffer_data = (char*)malloc(MP_SEGMENT_BUF_COUNT * MP_SEGMENT_SIZE);
		segments_ptr = new	segment_buffer_s[MP_SEGMENT_BUF_COUNT];
		for (mempage_int_t i = 0;i < MP_SEGMENT_BUF_COUNT;i++) {
			segment_buffer_t page = segments_ptr + i;
			page->buffer = segment_buffer_data + MP_SEGMENT_SIZE * i;
			page->mp = NULL;
			page->index = 0;

			segments_list.push_back(page);
		}
	}

	if (index >= mp->segment_count) return NULL;

	segment_buffer_list_t::iterator it = std::prev(segments_list.end());

	for (auto cit = segments_list.begin(); cit != segments_list.end(); cit++)
	{
		segment_buffer_t p = *cit;
		if (p->mp == NULL || p->mp != mp || p->index != index) {
			it = cit;
			break;
		}
	}

	segment_buffer_t buffer = *it;
	if (it != segments_list.begin()) {
		segments_list.erase(it);
		segments_list.push_front(buffer);
	}

	if (buffer->mp == mp && buffer->index == index) return buffer;

	uLong dst_size = MP_SEGMENT_SIZE;
    
	int ret = uncompress((Bytef*)buffer->buffer, &dst_size, (Bytef*)mp->data + mp->segment_offsets[index], mp->segment_sizes[index]);
	if (ret) {
		return NULL;
	}
    
    buffer->mp = mp;
    buffer->index = index;
    buffer->data_size = (mempage_int_t)dst_size;

	return buffer;
}

MEMPAGE_API mempage_t mempage_new(const char* data, mempage_int_t data_size) {
	mempage_t mp = new mempage_s();

	memcpy(&mp->segment_count, data, sizeof(mempage_int_t));
	data += sizeof(mempage_int_t);

	mp->segment_sizes = new mempage_int_t[mp->segment_count];
	memcpy(mp->segment_sizes, data, sizeof(mempage_int_t)*mp->segment_count);
	data += sizeof(mempage_int_t) * mp->segment_count;

	mp->segment_offsets = new mempage_int_t[mp->segment_count];
	mempage_int_t accumulated = 0;
	for (mempage_int_t i = 0;i < mp->segment_count;i++) {
		mp->segment_offsets[i] = accumulated;
		accumulated += mp->segment_sizes[i];
	}

	memcpy(&mp->data_size, data, sizeof(mempage_int_t));
	data += sizeof(mempage_int_t);

	mp->data = (char*)malloc(mp->data_size);
	memcpy(mp->data, data, mp->data_size);

	return mp;
}

MEMPAGE_API mempage_t mempage_from_file(const char* filename) {
	FILE* f = fopen(filename, "rb");
	if (!f) return NULL;

	mempage_t mp = new mempage_s();

	fread(&mp->segment_count, sizeof(mempage_int_t), 1, f);
	mp->segment_sizes = new mempage_int_t[mp->segment_count];
	fread(mp->segment_sizes, sizeof(mempage_int_t), mp->segment_count, f);
	mp->segment_offsets = new mempage_int_t[mp->segment_count];
	fread(&mp->data_size, sizeof(mempage_int_t), 1, f);
	mp->data = (char*)malloc(mp->data_size);
	fread(mp->data, 1, mp->data_size, f);
	
	mempage_int_t accumulated = 0;
	for (mempage_int_t i = 0;i < mp->segment_count;i++) {
		mp->segment_offsets[i] = accumulated;
		accumulated += mp->segment_sizes[i];
	}

	return mp;
}

MEMPAGE_API mempage_int_t mempage_size(mempage_t mp) {
	return mp->data_size;
}

MEMPAGE_API void mempage_delete(mempage_t mp) {
	if (!mp) return;
    
	if (mp->data)free(mp->data);
	if (mp->segment_sizes) delete[] mp->segment_sizes;
	if (mp->segment_offsets) delete[] mp->segment_offsets;
    
	mp->data = NULL;
	mp->segment_sizes = NULL;
	mp->segment_offsets = NULL;
}

MEMPAGE_API int mempage_extract(mempage_t mp, mempage_int_t offset, mempage_int_t size, char* out) {
	mempage_int_t offset_end = offset + size;
	segment_buffer_t seg;
	mempage_int_t cp_start, cp_end, cp_size, out_size = 0;
	for (mempage_int_t i = MP_SEGMENT_SIZE_FLOOR(offset), next = i + MP_SEGMENT_SIZE, max = MP_SEGMENT_SIZE_FLOOR(offset_end);
		i <= max;
		i = next, next += MP_SEGMENT_SIZE) {
		seg = extract_segment(mp, MP_SEGMENT_INDEX(i));
        
		if (!seg) return MEMPAGE_SEGMENT_ERR;
        
		if (seg->data_size < MP_SEGMENT_SIZE) {
			next = i + seg->data_size;
		}
        
		cp_start = i < offset ? offset : i;
		cp_end = offset_end < next ? offset_end : next;
		cp_size = cp_end - cp_start;

		memcpy(out + out_size, seg->buffer + (cp_start - i), cp_size);

		out_size += cp_size;
	}
	return MEMPAGE_OK;
}

MEMPAGE_API int mempage_write(const char* data, mempage_int_t data_size, const char* filename) {
    FILE* f = fopen(filename, "wb");
    
	char buffer[MP_SEGMENT_SIZE];

	mempage_int_t segment_count = MP_SEGMENT_TOTAL(data_size);
	mempage_int_t* segment_sizes = new mempage_int_t[segment_count];

	fwrite(&segment_count, sizeof(mempage_int_t), 1, f);
	fwrite(f, sizeof(mempage_int_t), segment_count, f);

	uLong pack_size;
	int ret = 0;
	mempage_int_t n = 0;
	uLong total_size = 0;
	fwrite(&total_size, sizeof(mempage_int_t), 1, f);
	for (mempage_int_t i = 0; i < data_size; i += MP_SEGMENT_SIZE) {
		pack_size = MP_SEGMENT_SIZE;
		ret = compress2((Bytef*)buffer, &pack_size, (const Bytef*)data + i, (i + MP_SEGMENT_SIZE < data_size ) ? MP_SEGMENT_SIZE : (data_size - i), Z_BEST_SPEED);
		if (ret) {
            fclose(f);
			delete[] segment_sizes;
			return MEMPAGE_BUFFER_SIZE_ERR;
		}
		segment_sizes[n++] = (mempage_int_t)pack_size;
		total_size += pack_size;
		fwrite(buffer, 1, (size_t)pack_size, f);
	}

	fseek(f, 0, SEEK_SET);

	fwrite(&segment_count, sizeof(mempage_int_t), 1, f);
	fwrite(segment_sizes, sizeof(mempage_int_t), segment_count, f);
	fwrite(&total_size, sizeof(mempage_int_t), 1, f);

	fseek(f, 0, SEEK_END);

	delete[] segment_sizes;

    fclose(f);
    
	return MEMPAGE_OK;
}
