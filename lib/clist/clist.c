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


struct CList
{
    clist_mem_allocator allocator;
    clist_mem_deallocator deallocator;
    clist_mem_deallocator data_deallocator;
    size_t size;
    CListNode* root;
};

CList* clist_create(clist_mem_allocator allocator, clist_mem_deallocator deallocator, clist_mem_deallocator data_deallocator)
{
    /* 分配CList内存 */
    auto clist = (CList*)allocator(sizeof(CList));
    /* 保存内存分配时和释放器 */
    clist->allocator = allocator;
    clist->deallocator = deallocator;
    clist->data_deallocator = data_deallocator;
    /* 创建根节点为空节点，方便管理 */
    clist->size = 0;
    clist->root = clist->allocator(sizeof(CListNode));
    /* 初始化根节点 */
    clist->root->prev = clist->root->next = clist->root;
    clist->root->data = nullptr;

    return clist;
}

void clist_destroy(CList* clist)
{
    /* 清除子节点 */
    while (clist_size(clist) > 0)
    {
        clist_pop_front(clist);
    }
    /* 清除根节点 */
    clist->deallocator(clist->root);
    clist->root = nullptr;
    clist->deallocator(clist);
}

CListIterator* clist_begin(CList* clist)
{
    return clist->root->next;
}

CListIterator* clist_end(CList* clist)
{
    return clist->root;
}

CListIterator* clist_insert(CList* clist, CListIterator* prev, void* data)
{
    ++clist->size;
    /* 创建新节点 */
    auto new_node = (CListNode*)clist->allocator(sizeof(CListNode));
    new_node->next = prev->next;
    new_node->prev = prev;
    new_node->data = data;

    prev->next = new_node;
    new_node->next->prev = new_node;

    return new_node;
}

void clist_pop(CList* clist, CListIterator* iter)
{
    // todo 检查list为空的情况
    --clist->size;
    /* 根节点恒为空，无法删除 */
    if (iter == clist->root)
        return;
    /* 处理前后节点的指向关系，前后节点一定和iter不是同一个节点 */
    iter->prev->next = iter->next;
    iter->next->prev = iter->prev;
    /* 取出数据 */
    void* data = iter->data;
    clist->data_deallocator(data);
    /* 清空无效指针 */
    iter->prev = iter->next = iter->data = nullptr;
    /* 释放节点内存 */
    clist->deallocator(iter);
}

CListIterator* clist_push_front(CList* clist, void* data)
{
    return clist_insert(clist, clist->root, data);
}

void clist_pop_front(CList* clist)
{
    clist_pop(clist, clist->root->next);
}

CListIterator* clist_push_back(CList* clist, void* data)
{
    return clist_insert(clist, clist->root->prev, data);
}

void clist_pop_back(CList* clist)
{
    clist_pop(clist, clist->root->prev);
}

size_t clist_size(CList* clist)
{
    return clist->size;
}

void* clist_front(CList* clist)
{
    return clist->root->next->data;
}

void* clist_back(CList* clist)
{
    return clist->root->prev->data;
}

CListIterator* clist_iterator_next(CListIterator* iter)
{
    return iter->next;
}

CListIterator* clist_iterator_prev(CListIterator* iter)
{
    return iter->prev;
}

void* clist_iterator_data(CListIterator* iter)
{
    return iter->data;
}
