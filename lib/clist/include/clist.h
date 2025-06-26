/**
  ******************************************************************************
  * @encoding  utf-8
  * @file      clist.h
  * @author    ZYX
  * @brief     None
  ******************************************************************************
  */

#ifndef CLIST_H
#define CLIST_H

/**
 * 前向声明使用到的节点数据结构
 * 这里不包含其头文件，对使用者隐藏具体的实现细节，加快编译速度，并且可以防止用户错用函数
 */
typedef struct CListNode CListIterator;

/**
 * 实现一个最基础的双向循环链表数据结构，可自定义内存分配与释放，但是不会管data的内存分配与释放
 * 这里同样使用前向声明
 */
typedef struct CList CList;

/**
 * CList用来分配内存的函数，通过在初始化时传入这个函数，clist可以自动在共享内存上分配内存
 */
typedef void*(*clist_mem_allocator)(size_t);
/**
 * CList用来释放内存的函数
 */
typedef void (*clist_mem_deallocator)(void*);

void clist_init(CList* clist, clist_mem_allocator allocator, clist_mem_deallocator deallocator);

CListIterator* clist_insert(CList* clist, CListIterator* prev, void* data);
void* clist_pop(CList* clist, CListIterator* iter);

CListIterator* clist_begin(CList* clist);
CListIterator* clist_end(CList* clist);

CListIterator* clist_push_front(CList* clist, void* data);
void* clist_pop_front(CList* clist);

CListIterator* clist_push_back(CList* clist, void* data);
void* clist_pop_back(CList* clist);

size_t clist_size(CList* clist);
void* clist_front(CList* clist);
void* clist_back(CList* clist);

#endif //CLIST_H
