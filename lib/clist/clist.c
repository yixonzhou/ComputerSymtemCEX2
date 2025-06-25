/**
  ******************************************************************************
  * @encoding  utf-8
  * @file      clist.c
  * @author    ZYX
  * @brief     None
  ******************************************************************************
  */

#include <stddef.h>
#include "clist.h"

typedef struct CListNode CListNode;

struct CListNode
{
    CListNode *next, *prev;
    void* data;
};

/**
 * 可以自定义内存分配的双向循环链表
 */
struct CList
{
    clist_mem_allocator allocator;
    clist_mem_deallocator deallocator;
    size_t size;
    CListNode *root;
};

void clist_init(CList* clist, clist_mem_allocator allocator, clist_mem_deallocator deallocator)
{
    /* 保存内存分配时和释放器 */
    clist->allocator = allocator;
    clist->deallocator = deallocator;
    /* 创建根节点为空节点，方便管理 */
    clist->size = 0;
    clist->root = clist->allocator(sizeof CListNode);
    /* 初始化根节点 */

}

CListIterator* clist_begin(CList* clist)
{
    return clist->root->next;
}

CListIterator* clist_end(CList* clist)
{
    return clist->root;
}

void clist_push_front(CList* clist, void* data)
{
    /* 创建新节点 */
    auto new_node = clist->allocator(sizeof CListNode);
    clist_node_insert(new_node, new_node, clist->root, data);
}
