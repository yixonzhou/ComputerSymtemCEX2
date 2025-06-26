/**
  ******************************************************************************
  * @encoding  utf-8
  * @file      myfilesystem.c
  * @author    ZYX
  * @brief     None
  ******************************************************************************
  */

#include "myfilesystem.h"

#include <stdio.h>
#include <stdlib.h>

#include "clist.h"
#include <sys/ipc.h>
#include <sys/shm.h>


typedef struct FileSystemMemoryMetadata FileSystemMemoryMetadata;

/**
 * 存储内存的元数据，包含内存的实际大小信息和内存实际可用的地址
 */
struct FileSystemMemoryMetadata
{
    size_t size;
    void* address;
};

struct FileSystemNode
{
    char type;
    void* data;
};

struct FileSystem
{
    size_t magic_number; /* 辅助判断该共享内存是不是第一次创建, 只有创建时可以修改，其余时候只读 */
    size_t shm_offset; /* 记录当前使用的共享内存偏移量 */
    CList* root;
    CList* unused_nodes;
};

const int SHM_SIZE = 100 * 1024 * 1024;
const key_t SHM_KEY = 12345;
const size_t MAGIC_NUMBER = 0xDEADBEEF;

/* 共享内存的首地址存储该结构 */
FileSystem* f;

void* get_offset_address()
{
    return (char*)f + f->shm_offset;
}

void* alloc_memory(size_t size)
{
    size_t size_with_metadata = size + sizeof(FileSystemMemoryMetadata);
    /* 检查是否有空闲内存的大小正好可以容纳新分配的内存 */
    for (auto it = clist_begin(f->unused_nodes); it != clist_end(f->unused_nodes); it = clist_iterator_next(it))
    {
        auto unused_mem_metadata = (FileSystemMemoryMetadata*)clist_iterator_data(it);
        /* 检查这块内存是否满足要求 */
        if (unused_mem_metadata->size >= size_with_metadata)
        {
            /* 移出未使用列表, 注意，这一步会导致迭代器不可用，不能继续迭代 */
            clist_pop(f->unused_nodes, it);
            return unused_mem_metadata->address;
        }
    }

    /* 分配的内存的头部记录内存的地址和大小信息 */
    // todo 检测内存移出的问题
    auto metadata = (FileSystemMemoryMetadata*)get_offset_address();
    f->shm_offset += sizeof(FileSystemMemoryMetadata);

    metadata->size = size_with_metadata;
    metadata->address = get_offset_address(size);
    f->shm_offset += size;

    return metadata->address;
}

void free_memory(void* mem)
{
    /* 获取内存块metadata */
    auto metadata = (FileSystemMemoryMetadata*)((char*)mem - sizeof(FileSystemMemoryMetadata));
    clist_push_back(f->unused_nodes, metadata);
}

void filesystem_init()
{
    /* 创建或者获取共享内存 */
    int shmid = shmget(SHM_KEY, SHM_SIZE, 0644);
    if (shmid == -1)
    {
        perror("shmget failed");
        exit(1);
    }
    /* 附加到内存空间并取出共享内存中的文件系统 */
    f = (FileSystem*)shmat(shmid, nullptr, 0);
    /* 检查是否是未初始化的文件系统 */
    if (f->magic_number != MAGIC_NUMBER)
    {
        /* 初始化共享内存 */
        f->magic_number = MAGIC_NUMBER;
        f->shm_offset = sizeof(FileSystem);
        f->root = nullptr;
        f->unused_nodes = nullptr;

        /* 创建根目录 */
        f->root =
    }
}
