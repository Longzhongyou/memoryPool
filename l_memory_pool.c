/***********************************************
 * author: longzy
 * date: 20190510
 * ������ �ο�c++ stl v3.3�ķ�����ʵ�֣�
 *        Ȼ���Լ��������c��ʵ�֣���Ҫѧϰ��
 *        �����ڴ�ص�ʵ�ּ���
 ***********************************************/
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include "l_memory_pool.h"


#define __ALIGN 8                               //������ϵ��߽�
#define __MAX_BYTES  128                        //������������
#define __NFREELISTS (__MAX_BYTES)/(__ALIGN)    //list �ĸ���
#define __NOBJS 20                              //

typedef struct __obj
{
	struct __obj *free_list_link;
}obj;

char *start_free = 0;      //��ʼλ��
char *end_free = 0;        //����λ��
size_t heap_size = 0;      //list���ۻ�������
obj *free_list[__NFREELISTS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


/******һ����������ʵ��****/
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


/**********����������ʵ��************/
//����bytes����������8�ı���
//eg: 5->8; 12->16
static size_t ROUND_UP(size_t bytes)
{
	return (((bytes)+__ALIGN - 1) & ~(__ALIGN - 1));
}

//����bytes�ҵ���Ӧ��freelist������±�λ��
//eg : {8,16,24,32,,,,,,128}
static size_t FREELIST_INDEX(size_t bytes) {
	return (((bytes)+__ALIGN - 1) / __ALIGN - 1);
}

static char *chunk_alloc(size_t size,int *objs)
{
	size_t total_bytes = size * (*objs);  //��Ҫ���ܴ�С
	size_t bytes_left = end_free - start_free; //ʣ�µĴ�С

	//1�����ʣ�µĴ�С������Ҫ���ܴ�С��ֱ�ӷ���
	//2�����ʣ�µĴ�С����һ��obj�Ĵ�Сn����ô����obj��ֱ�ӷ���
	//3����������������������·���
	void *result = 0;
	if (bytes_left > total_bytes)   //�����ĵݹ���û�������return
	{
		result = start_free;    //���ؿ�ʼ��λ��
		start_free = start_free + total_bytes; //����start_free��λ��
		return result;
	}
	else if (bytes_left > size)
	{
		*objs = bytes_left / size;    //���¼����ʣ�µ�bytes�ܹ�������ٸ�obj
		total_bytes = size * (*objs);  //��Ҫ�ܴ�С
		result = start_free;
		start_free = start_free + total_bytes;  //����start_freeλ��
		return result;
	}
	else
	{
		//1���������ʣ��ģ������ڴ���Ƭ
		//2������Ҫ�����µĴ�С
		//3�������µ��ڴ�
		//4���������ʧ��
		//5������ɹ���ݹ����chunk_alloc����

		if (bytes_left > 0)
		{
			//start_free����һ���ڴ�ָ�����ʣ��Ŀռ�
			obj *volatile *my_free_list = free_list + FREELIST_INDEX(bytes_left);
			((obj*)start_free)->free_list_link = *my_free_list;
			*my_free_list = (obj*)start_free;
		}

		size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);

		start_free = (char*)malloc(bytes_to_get);
#ifdef DEBUG
		ncount++; //���Ե�ʱ�򿴵����˶��ٴ�malloc
#endif

		if (0 == start_free) //����ʧ��
		{
			//1,�ӵ�ǰλ�������ң����ܷ��ҵ�һ���пռ�ĵ�ַ
			//2,����ҵ��Ļ����ݹ���÷���
			//3,����Ҳ���������һ����������Ȼ����÷���ʧ�ܴ�����������
			obj *volatile *my_free_list;
			obj *p;
			for (int i = size; i < __MAX_BYTES; i += __ALIGN)
			{
				my_free_list = free_list + FREELIST_INDEX(i);
				p = *my_free_list;
				if (0 != p) //�пռ�
				{
					*my_free_list = p->free_list_link;
					start_free = (char*)p;
					end_free = start_free + i;
					return chunk_alloc(size, objs);   //�ݹ���ã���ʣ��ռ������Ҫ�Ŀռ��ʣ��ռ��㹻һ��objʱ����
				}
			}

			//ѭ�����û�ҵ�
			end_free = 0;       //In case of exception.
			start_free = (char*)malloc_allocate(bytes_to_get); //ʹ��һ��������
		}

		heap_size += bytes_to_get;  //�ۻ�������
		end_free = start_free + bytes_to_get;  //����end_free��λ��
		return chunk_alloc(size, objs);   //�ݹ���ã���ʣ��ռ������Ҫ�Ŀռ��ʣ��ռ��㹻һ��objʱ����
	}
}

static void *refill(size_t n)
{
	int nobjs = __NOBJS;
	char *chunk = chunk_alloc(n,&nobjs); //chunkһ���ռ�

	if (1 == nobjs) //���ֻ��һ��obj����ôֱ�ӷ���
	{
		return (void *)chunk;
	}

	//���nobj����1����ѷ����chunk�ֳ�obj�����飬Ȼ������
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
	//1,���n�Ƿ񳬹�ÿ��������������ֵ__MAX_BYTES,�����������һ��������
	if (size > __MAX_BYTES)
	{
		return malloc_allocate(size);
	}
	//2,�ҵ�n��free_list����λ��,���û�����·���ռ�,����ֱ�ӷ���
	obj *volatile *my_free_list = free_list + FREELIST_INDEX(size);
	obj *result = *my_free_list;
	if (0 == result)
	{
		//���·���������Ȼ�󷵻�
		void *r = refill(ROUND_UP(size));
		return r;
	}
	//���ص�ǰλ�ã�Ȼ���ƶ�����һ��λ��
	*my_free_list = result->free_list_link;
	return (void *)(result);
}

void *l_reallocate(void *p, size_t old_sz, size_t new_sz)
{
	//�ж�old_sz��new_sz�Ƿ񳬹�����
	if (old_sz > __MAX_BYTES && new_sz > __MAX_BYTES)
	{
		return realloc(p,new_sz); //�������޺�ֱ�ӵ���realloc
	}
	//�ж�old_sz��new_sz�Ƿ���free_list�е�ͬһλ��
	if (ROUND_UP(old_sz) == ROUND_UP(new_sz))
	{
		return p;//ֱ�ӷ���
	}
	//����l_allcate���·���һ��ռ�
	void *result = l_allocate(new_sz);
	//��ֵold���µĿռ�
	size_t copy_sz = new_sz > old_sz ? old_sz : new_sz;
	memcpy(result, p, copy_sz);

	//�ͷŵ�old
	l_deallocate(p,old_sz);
	return result;
}

void l_deallocate(void *p, size_t size)
{
	//���n��������ֵ������һ��������
	if (size > __MAX_BYTES)
	{
		return malloc_deallocate(p, size);
	}

	//Ȼ��� p �黹��free_list
	obj *q = (obj*)p;
	obj *volatile *my_free_list = free_list + FREELIST_INDEX(size);
	q->free_list_link = *my_free_list;
	*my_free_list = q;
}
