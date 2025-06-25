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


struct FileSystemNone
{
    char type;
    void *data;
};


struct FileSystem
{
    size_t magic_number; /* 辅助判断该共享内存是不是第一次创建, 只有创建时可以修改，其余时候只读 */
    size_t shm_offset;  /* 记录当前使用的共享内存偏移量 */
    CList *root;
    CList *unused_nodes;
};

const int SHM_SIZE = 100 * 1024 * 1024;
const key_t SHM_KEY = 12345;
const size_t MAGIC_NUMBER = 0xDEADBEEF;

/* 共享内存的首地址存储该结构 */
FileSystem *f;

void *get_offset_address(FileSystem *f)
{
    return (char *)f + f->shm_offset;
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
    f = (FileSystem *)shmat(shmid, nullptr, 0);
    /* 检查是否是未初始化的文件系统 */
    if (f->magic_number != MAGIC_NUMBER)
    {
        /* 初始化共享内存 */
        f->magic_number = MAGIC_NUMBER;
        f->shm_offset = sizeof(FileSystem);
        f->root = nullptr;
        f->unused_nodes = nullptr;

        /* 创建根目录 */
    }
}


