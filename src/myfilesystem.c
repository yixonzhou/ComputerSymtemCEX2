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
#include <pthread.h>
#include <string.h>

constexpr size_t FILESYSTEM_NODE_NAME_SIZE = 100;
char filesystem_error[FILESYSTEM_NODE_NAME_SIZE];

typedef struct FileSystemNode FileSystemNode;

typedef enum FileSystemNodeType
{
    Unknown = -1,
    File = 0,
    Directory,
} FileSystemNodeType;

/**
 * 存储内存的元数据，包含内存的实际大小信息和内存实际可用的地址
 */
typedef struct FileSystemMemoryMetadata
{
    size_t size;
    void* address;
} FileSystemMemoryMetadata;

typedef struct FileSystemNode
{
    FileSystemNode* parent; /* 父节点指针 */
    FileSystemNodeType type; /* 节点类型 */
    char name[FILESYSTEM_NODE_NAME_SIZE]; /* 文件或路径名 */
    void* data; /* 对于目录，这个是一个CList, 存储子节点; 对于文件，这里存储文件数据 */
} FileSystemNode;

struct FileSystem
{
    size_t magic_number; /* 辅助判断该共享内存是不是第一次创建, 只有创建时可以修改，其余时候只读 */
    pthread_rwlock_t rwlock; /* 读写锁，并行时同步使用，运行同时读，不允许同时写 */
    size_t shm_offset; /* 记录当前使用的共享内存偏移量 */
    CList* unused_nodes; /* 分配后被释放的内存 */
    FileSystemNode* root; /* 根目录 */
    FileSystemNode* cur_dir; /* 当前目录 */
    size_t pwd_offset;
    char pwd[FILESYSTEM_NODE_NAME_SIZE * 10]; /* 当前目录路径 */
};

const int SHM_SIZE = 100 * 1024 * 1024;
const key_t SHM_KEY = 12345;
const size_t MAGIC_NUMBER_INITED = 0xDEADBEEF, MAGIC_NUMBER_DEINITED = ~MAGIC_NUMBER_INITED;
const void *SHM_ADDR = (void *)0x0000700000000000;

/* 共享内存的首地址存储该结构 */
FileSystem* f = nullptr;

/**
 * 获取当前空内存指针，禁止外部调用
 * @return 当前空内存指针
 */
static void* _get_offset_address()
{
    return (char*)f + f->shm_offset;
}

/**
 * 获取最近一块未被分配的内存，并将空内存指针偏移size个字节，不会复用已释放内存，禁止外部调用
 * @param size 需要便宜的内存大小
 * @return 获取的内存地址
 */
static void* _get_and_offset_address(size_t size)
{
    void* address = _get_offset_address();
    f->shm_offset += size;
    return address;
}

void* alloc_memory(size_t size)
{
    /* 写锁 */
    pthread_rwlock_wrlock(&f->rwlock);
    size_t size_with_metadata = size + sizeof(FileSystemMemoryMetadata);
    /* 检查是否有空闲内存的大小正好可以容纳新分配的内存, 特别的，文件系统未完全初始化时没有unused_nodes */
    if (f->unused_nodes != nullptr) {
        for (auto it = clist_begin(f->unused_nodes); it != clist_end(f->unused_nodes); it = clist_iterator_next(it)) {
            auto unused_mem_metadata = (FileSystemMemoryMetadata*)clist_iterator_get(it);
            /* 检查这块内存是否满足要求 */
            if (unused_mem_metadata->size >= size_with_metadata) {
                /* 移出未使用列表, 注意，这一步会导致迭代器不可用，不能继续迭代 */
                clist_pop(f->unused_nodes, it);
                return unused_mem_metadata->address;
            }
        }
    }

    // todo 检测内存移出的问题
    /* 分配的内存的元数据 */
    auto metadata = (FileSystemMemoryMetadata*)_get_and_offset_address(sizeof(FileSystemMemoryMetadata));
    metadata->size = size_with_metadata;
    /* 分配实际需要的内存 */
    metadata->address = _get_and_offset_address(size);

    pthread_rwlock_unlock(&f->rwlock);
    return metadata->address;
}

void free_memory(void* mem)
{
    if (mem == nullptr)
        return;
    pthread_rwlock_wrlock(&f->rwlock);
    /* 获取内存块metadata */
    auto metadata = (FileSystemMemoryMetadata*)((char*)mem - sizeof(FileSystemMemoryMetadata));
    /* 添加到未使用的内存列表 */
    clist_push_back(f->unused_nodes, metadata);
    pthread_rwlock_unlock(&f->rwlock);
}

void filesystem_node_destroy(FileSystemNode* node)
{
    if (node == nullptr || node == f->root)
        return;
    pthread_rwlock_wrlock(&f->rwlock);
    // 清除data
    if (node->type == File) {
        free_memory(node->data);
    }
    else if (node->type == Directory) {
        // 摧毁所有子节点
        auto subnode_list = (CList*)node->data;
        for (auto it = clist_begin(subnode_list); it != clist_end(subnode_list); it = clist_iterator_next(it)) {
            auto subnode = (FileSystemNode*)clist_iterator_get(it);
            if (subnode != nullptr) {
                filesystem_node_destroy(clist_iterator_get(it));
            }
        }
    }
    else {
        // todo 未知类型
    }
    // 更新parent的指针
    auto parent_subnode_list = (CList*)node->parent->data;
    for (auto it = clist_begin(parent_subnode_list); it != clist_end(parent_subnode_list); it =
         clist_iterator_next(it)) {
        auto subnode = (FileSystemNode*)clist_iterator_get(it);
        if (subnode == node) {
            clist_iterator_set(it, nullptr);
            break;
        }
    }
    // 清除node数据并释放内存
    node->parent = nullptr;
    node->type = Unknown;
    node->name[0] = '\0';
    node->data = nullptr;
    free_memory(node);
}

/**
 * 创建一个节点
 * @param parent 父节点指针
 * @param type 节点类型
 * @param name 节点名称
 * @param data 节点数据，如果时目录会忽略此参数
 * @return 创建后的节点指针
 */
FileSystemNode* filesystem_node_create(FileSystemNode* parent, FileSystemNodeType type, const char* name, void* data)
{
    if (parent == nullptr) {
        return nullptr;
    }
    // todo 检查参数合法性
    auto node = (FileSystemNode*)alloc_memory(sizeof(FileSystemNode));
    node->parent = parent;
    node->type = type;
    strcpy(node->name, name);
    if (node->type == File) {
        node->data = data;
    }
    else if (node->type == Directory) {
        /* 创建一个空的目录链表 */
        node->data = clist_create(alloc_memory, free_memory, filesystem_node_destroy);
    }
    else {
        // todo 未知类型
        free_memory(node);
        return nullptr;
    }
    return node;
}


void filesystem_init()
{
    /* 创建或者获取共享内存 */
    int shmid = shmget(SHM_KEY, SHM_SIZE, 0644);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    /* 附加到内存空间并取出共享内存中的文件系统 */
    f = (FileSystem*)shmat(shmid, SHM_ADDR, 0);
    if (f != SHM_ADDR) {
        perror("shmat failed");
        exit(1);
    }
    /* 检查是否是未初始化的文件系统 */
    if (f->magic_number != MAGIC_NUMBER_INITED) {
        // todo 创建文件系统时没有做并行的同步

        /* 初始化共享内存 */
        f->magic_number = MAGIC_NUMBER_INITED;
        /* 初始化读写锁 */
        pthread_rwlockattr_t attr;
        // 初始化读写锁属性
        if (pthread_rwlockattr_init(&attr) != 0) {
            perror("pthread_rwlockattr_init");
            exit(EXIT_FAILURE);
        }
        // 设置为进程间共享
        if (pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
            perror("pthread_rwlockattr_setpshared");
            exit(EXIT_FAILURE);
        }
        // 初始化读写锁
        if (pthread_rwlock_init(&f->rwlock, &attr) != 0) {
            perror("pthread_rwlock_init");
            exit(EXIT_FAILURE);
        }
        // 销毁属性对象
        pthread_rwlockattr_destroy(&attr);

        /* 设置文件系统初始值 */
        f->shm_offset = sizeof(FileSystem);
        f->unused_nodes = nullptr;
        f->root = nullptr;
        f->cur_dir = nullptr;

        /* 创建释放后的内存列表 */
        f->unused_nodes = clist_create(alloc_memory, free_memory, filesystem_node_destroy);

        /* 创建根目录 */
        f->root = filesystem_node_create(nullptr, Directory, "/", nullptr);
        f->cur_dir = f->root;
        /* 初始化pwd */
        f->pwd_offset = 1;
        f->pwd[0] = '/';
        f->pwd[1] = '\0';
    }
}

void filesystem_deinit()
{
    if (f == nullptr)
        return;
    /* 防止有进程在使用 */
    pthread_rwlock_wrlock(&f->rwlock);
    /* 防止后续重新分配到这块内存时被误认为已初始化 */
    f->magic_number = MAGIC_NUMBER_DEINITED;
    pthread_rwlock_unlock(&f->rwlock);
    // todo 销毁过程中又有进程使用共享内存怎么办
    /* 反初始化共享锁 */
    pthread_rwlock_destroy(&f->rwlock);
    /* 释放共享内存 */
    // 分离共享内存
    if (shmdt(f) == -1) {
        perror("shmdt failed");
    }

    // 3. 获取共享内存ID以便删除
    int shmid = shmget(SHM_KEY, SHM_SIZE, 0644);
    if (shmid == -1) {
        perror("shmget for deletion failed");
        return;
    }

    // 4. 标记共享内存为待删除
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl IPC_RMID failed");
    }
}

void cd(const char* path)
{
    static size_t pos;
    static char name[FILESYSTEM_NODE_NAME_SIZE];

    // 保留当前目录，方便查找错误后的回溯
    auto ori_dir = f->cur_dir;

    // 特殊处理绝对目录
    pos = 0;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' ||path[i] == '\\') {
            // 处理这一级目录操作
            name[pos] = '\0';
            if (strcmp(name, ".") == 0) {
                // 当前目录, 不变
            } else if (strcmp(name, "..") == 0) {
                // 上一级目录
                if (f->cur_dir != f->root) {
                    f->cur_dir = f->cur_dir->parent;
                } else {
                    // 根目录的上一级不变
                }
            } else {
                // 查找是否存在该子目录
                bool find = false;
                auto subnode_list = (CList*)f->cur_dir->data;
                for (auto it = clist_begin(subnode_list); it != clist_end(subnode_list); it = clist_iterator_next(it)) {
                    auto subnode = (FileSystemNode*)clist_iterator_get(it);
                    if (subnode == nullptr)
                        continue;
                    if (strcmp(name, subnode->name) == 0) {
                        find = true;
                        f->cur_dir = subnode;
                        break;
                    }
                }
                if (!find) {
                    // 发生错误，回溯结构并报错
                    f->cur_dir = ori_dir;
                    sprintf(filesystem_error, "cd error, dir \"%s\" not exist!", path);
                    perror(filesystem_error);
                    exit(1);
                }
            }
            // 复位name
            pos = 0;
        }
    }
}
