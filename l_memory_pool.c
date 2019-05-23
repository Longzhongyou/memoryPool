/***********************************************
 * author: longzy
 * date: 20190510
 * 描述： 参考c++ stl v3.3的分配器实现，
 *        然后按自己的理解用c来实现，主要学习了
 *        里面内存池的实现技术
 ***********************************************/
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include "l_memory_pool.h"


#define __ALIGN 8                               //区块的上调边界
#define __MAX_BYTES  128                        //区块的最大上限
#define __NFREELISTS (__MAX_BYTES)/(__ALIGN)    //list 的个数
#define __NOBJS 20                              //

typedef struct __obj
{
	struct __obj *free_list_link;
}obj;

char *start_free = 0;      //起始位置
char *end_free = 0;        //结束位置
size_t heap_size = 0;      //list的累积分配量
obj *free_list[__NFREELISTS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


/******一级分配器的实现****/
#define _THROW_BAD_ALLOC_ exit(1)
void(*malloc_handler)();

static void *oom_malloc(size_t n)
{
	void(*my_malloc_handler)();
	void *result;
	for (; ;)
	{
		my_malloc_handler = malloc_handler;
		if (0 == my_malloc_handler)
		{
			_THROW_BAD_ALLOC_;
		}
		(*my_malloc_handler)();
		result = malloc(n);
		if (result)
		{
			return result;
		}
	}
}

static void *oom_realloc(void *p, size_t new_sz)
{
	void(*my_malloc_handler)();
	void *result;
	for (; ;)
	{
		my_malloc_handler = malloc_handler;
		if (0 == my_malloc_handler)
		{
			_THROW_BAD_ALLOC_;
		}
		result = realloc(p, new_sz);
		if (result)
		{
			return result;
		}
	}
}

void(*set_malloc_handler(void(*f)()))
{
	void(*old)() = malloc_handler;
	malloc_handler = f;
	return old;
}

void *malloc_allocate(size_t n)
{
	void *result = malloc(n);
#ifdef DEBUG
	ncount++;
#endif // DEBUG

	if (result == NULL)
	{
		result = oom_malloc(n);
	}
	return result;
}

void malloc_deallocate(void *p, size_t n)
{
	free(p);
}

static void *malloc_reallocate(void *p, size_t old_sz, size_t new_sz)
{
	void *result = realloc(p, new_sz);
	if (result == NULL)
	{
		result = oom_realloc(p, new_sz);
	}
	return result;
}


/**********二级分配器实现************/
//根据bytes调整上升到8的倍数
//eg: 5->8; 12->16
static size_t ROUND_UP(size_t bytes)
{
	return (((bytes)+__ALIGN - 1) & ~(__ALIGN - 1));
}

//根据bytes找到对应的freelist里面的下标位置
//eg : {8,16,24,32,,,,,,128}
static size_t FREELIST_INDEX(size_t bytes) {
	return (((bytes)+__ALIGN - 1) / __ALIGN - 1);
}

static char *chunk_alloc(size_t size,int *objs)
{
	size_t total_bytes = size * (*objs);  //需要的总大小
	size_t bytes_left = end_free - start_free; //剩下的大小

	//1，如果剩下的大小大于需要的总大小，直接返回
	//2，如果剩下的大小大于一个obj的大小n，那么调整obj后直接返回
	//3，不满足上面的条件就重新分配
	void *result = 0;
	if (bytes_left > total_bytes)   //后续的递归调用会在这里return
	{
		result = start_free;    //返回开始的位置
		start_free = start_free + total_bytes; //调整start_free的位置
		return result;
	}
	else if (bytes_left > size)
	{
		*objs = bytes_left / size;    //重新计算的剩下的bytes能够分配多少个obj
		total_bytes = size * (*objs);  //需要总大小
		result = start_free;
		start_free = start_free + total_bytes;  //调整start_free位置
		return result;
	}
	else
	{
		//1，如果还有剩余的，整理内存碎片
		//2，计算要分配新的大小
		//3，分配新的内存
		//4，处理分配失败
		//5，分配成功后递归调用chunk_alloc返回

		if (bytes_left > 0)
		{
			//start_free的下一块内存指向这块剩余的空间
			obj *volatile *my_free_list = free_list + FREELIST_INDEX(bytes_left);
			((obj*)start_free)->free_list_link = *my_free_list;
			*my_free_list = (obj*)start_free;
		}

		size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);

		start_free = (char*)malloc(bytes_to_get);
#ifdef DEBUG
		ncount++; //调试的时候看调用了多少次malloc
#endif

		if (0 == start_free) //分配失败
		{
			//1,从当前位置往后找，看能否找到一块有空间的地址
			//2,如果找到的话，递归调用返回
			//3,如果找不到，调用一级分配器，然后调用分配失败处理函数来处理
			obj *volatile *my_free_list;
			obj *p;
			for (int i = size; i < __MAX_BYTES; i += __ALIGN)
			{
				my_free_list = free_list + FREELIST_INDEX(i);
				p = *my_free_list;
				if (0 != p) //有空间
				{
					*my_free_list = p->free_list_link;
					start_free = (char*)p;
					end_free = start_free + i;
					return chunk_alloc(size, objs);   //递归调用，在剩余空间大于需要的空间或剩余空间足够一个obj时返回
				}
			}

			//循环完后还没找到
			end_free = 0;       //In case of exception.
			start_free = (char*)malloc_allocate(bytes_to_get); //使用一级分配器
		}

		heap_size += bytes_to_get;  //累积分配量
		end_free = start_free + bytes_to_get;  //调整end_free的位置
		return chunk_alloc(size, objs);   //递归调用，在剩余空间大于需要的空间或剩余空间足够一个obj时返回
	}
}

static void *refill(size_t n)
{
	int nobjs = __NOBJS;
	char *chunk = chunk_alloc(n,&nobjs); //chunk一大块空间

	if (1 == nobjs) //如果只够一个obj，那么直接返回
	{
		return (void *)chunk;
	}

	//如果nobj大于1，则把分配的chunk分成obj个区块，然后串起来
	obj *volatile * my_free_list = free_list + FREELIST_INDEX(n);
	char *result = chunk;
	obj *current_obj, *next_obj;

	*my_free_list = next_obj = (obj *)(chunk + n);
	for (int i = 1; ; i++)
	{
		current_obj = next_obj;
		next_obj = (obj *)((char*)next_obj + n);;
		if (nobjs - 1 == i)
		{
			current_obj->free_list_link = 0;
			break;
		}
		else
		{
			current_obj->free_list_link = next_obj;
		}
	}
	return (void *)result;
}

void *l_allocate(size_t size)
{
	//1,检测n是否超过每个区块的最大上限值__MAX_BYTES,如果超过交给一级分配器
	if (size > __MAX_BYTES)
	{
		return malloc_allocate(size);
	}
	//2,找到n在free_list的中位置,如果没有重新分配空间,否则直接返回
	obj *volatile *my_free_list = free_list + FREELIST_INDEX(size);
	obj *result = *my_free_list;
	if (0 == result)
	{
		//重新分配或调整，然后返回
		void *r = refill(ROUND_UP(size));
		return r;
	}
	//返回当前位置，然后移动到下一个位置
	*my_free_list = result->free_list_link;
	return (void *)(result);
}

void *l_reallocate(void *p, size_t old_sz, size_t new_sz)
{
	//判断old_sz和new_sz是否超过上限
	if (old_sz > __MAX_BYTES && new_sz > __MAX_BYTES)
	{
		return realloc(p,new_sz); //超过上限后直接调用realloc
	}
	//判断old_sz和new_sz是否在free_list中的同一位置
	if (ROUND_UP(old_sz) == ROUND_UP(new_sz))
	{
		return p;//直接返回
	}
	//调用l_allcate重新分配一块空间
	void *result = l_allocate(new_sz);
	//赋值old到新的空间
	size_t copy_sz = new_sz > old_sz ? old_sz : new_sz;
	memcpy(result, p, copy_sz);

	//释放掉old
	l_deallocate(p,old_sz);
	return result;
}

void l_deallocate(void *p, size_t size)
{
	//如果n大于上限值，则用一级分配器
	if (size > __MAX_BYTES)
	{
		return malloc_deallocate(p, size);
	}

	//然后把 p 归还给free_list
	obj *q = (obj*)p;
	obj *volatile *my_free_list = free_list + FREELIST_INDEX(size);
	q->free_list_link = *my_free_list;
	*my_free_list = q;
}
