#pragma once

/***********************************************
 * author: longzy
 * date: 20190510 
 * 描述： 提供对外调用的函数
 ***********************************************/

#ifndef _L_MEMORY_POOL_H_
#define _L_MEMORY_POOL_H_

#include <stdio.h>

#define DEBUG 1

int ncount;


void *malloc_allocate(size_t n);
void malloc_deallocate(void *p, size_t n);

void *l_allocate(size_t size);
void *l_reallocate(void *p, size_t old_sz, size_t new_sz);
void l_deallocate(void *p, size_t size);

#endif 