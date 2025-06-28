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
#include <stdarg.h>

#include "clist.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <string.h>

constexpr bool DEBUG = false;

constexpr size_t FILESYSTEM_NODE_NAME_SIZE = 100;
constexpr size_t FILESYSTEM_PWD_SIZE = FILESYSTEM_NODE_NAME_SIZE * 10;
char filesystem_error[FILESYSTEM_NODE_NAME_SIZE];

typedef struct FileSystemNode FileSystemNode;

typedef enum FileSystemNodeType
{
    Unknown = -1,
    File = 0,
    Directory,
} FileSystemNodeType;

const char* FileSystemNodeTypeNames[] = {
    "file",
    "directory",
};

/**
 * 存储内存的元数据，包含内存的实际大小信息和内存实际可用的地址
 */
typedef struct FileSystemMemoryMetadata
{
    size_t size;
    void* address;
} FileSystemMemoryMetadata;

struct FileSystemNode
{
    FileSystemNode* parent; /* 父节点指针 */
    FileSystemNodeType type; /* 节点类型 */
    char name[FILESYSTEM_NODE_NAME_SIZE]; /* 文件或路径名 */
    void* data; /* 对于目录，这个是一个CList, 存储子节点; 对于文件，这里存储文件数据 */
};

struct FileSystem
{
    size_t magic_number; /* 辅助判断该共享内存是不是第一次创建, 只有创建时可以修改，其余时候只读 */
    pthread_rwlock_t rwlock; /* 读写锁，并行时同步使用，运行同时读，不允许同时写 */
    size_t shm_offset; /* 记录当前使用的共享内存偏移量 */
    CList* unused_nodes; /* 分配后被释放的内存 */
    FileSystemNode* root; /* 根目录 */
    FileSystemNode* cur_dir; /* 当前目录 */
    size_t pwd_offset;
    char pwd[FILESYSTEM_PWD_SIZE]; /* 当前目录路径 */
};

const int SHM_SIZE = 100 * 1024 * 1024;
constexpr int DEBUG_FORMAT_SIZE = 1024;
const size_t MAGIC_NUMBER_INITED = 0xDEADBEEF, MAGIC_NUMBER_DEINITED = ~MAGIC_NUMBER_INITED;
const void* SHM_ADDR = (void*)0x0000700000000000;
int shmid = 0;

/* 共享内存的首地址存储该结构 */
FileSystem* f = nullptr;

int debug_printf(const char* format, ...)
{
    if (DEBUG) {
        static char debug_format[DEBUG_FORMAT_SIZE];
        va_list args;
        va_start(args);
        // 生成新的格式化字符串, 改变字体颜色
        sprintf(debug_format, "\033[34m%s\033[0m", format);
        return vprintf(debug_format, args);
    }
}

FileSystemNode* filesystem_node_get_subnode(FileSystemNode* node, FileSystemNodeType subnode_type,
                                            const char* subnode_name)
{
    auto subnode_list = (CList*)node->data;
    for (auto it = clist_begin(subnode_list); it != clist_end(subnode_list); it = clist_iterator_next(it)) {
        auto subnode = (FileSystemNode*)clist_iterator_get(it);
        if (subnode == nullptr)
            continue;
        if (subnode->type == subnode_type && strcmp(subnode->name, subnode_name) == 0) {
            return subnode;
        }
    }
    return nullptr;
}

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
    return metadata->address;
}

void free_memory(void* mem)
{
    if (mem == nullptr)
        return;
    /* 获取内存块metadata */
    auto metadata = (FileSystemMemoryMetadata*)((char*)mem - sizeof(FileSystemMemoryMetadata));
    /* 添加到未使用的内存列表 */
    clist_push_back(f->unused_nodes, metadata);
}

void filesystem_node_destroy(FileSystemNode* node)
{
    if (node == nullptr || node == f->root)
        return;
    // 清除data
    if (node->type == File) {
        free_memory(node->data);
    } else if (node->type == Directory) {
        // 摧毁所有子节点
        auto subnode_list = (CList*)node->data;
        for (auto it = clist_begin(subnode_list); it != clist_end(subnode_list); it = clist_iterator_next(it)) {
            auto subnode = (FileSystemNode*)clist_iterator_get(it);
            if (subnode != nullptr) {
                filesystem_node_destroy(clist_iterator_get(it));
            }
        }
    } else {
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
    // todo 检查参数合法性
    // 检查父节点是否有同名节点
    if (parent != nullptr) {
        auto subnode = filesystem_node_get_subnode(parent, type, name);
        if (subnode != nullptr) {
            printf("已有同名同类型节点 \"%s\"\n", name);
            return nullptr;
        }
    }

    auto node = (FileSystemNode*)alloc_memory(sizeof(FileSystemNode));
    node->parent = parent;
    node->type = type;
    strcpy(node->name, name);
    if (node->type == File) {
        node->data = data;
    } else if (node->type == Directory) {
        /* 创建一个空的目录链表 */
        node->data = clist_create();
    } else {
        // todo 未知类型
        free_memory(node);
        return nullptr;
    }
    // 更新父节点的子节点列表
    if (parent != nullptr) {
        auto parent_subnode_list = (CList*)parent->data;
        clist_push_back(parent_subnode_list, node);
    }
    return node;
}


void filesystem_init(const char* program_path)
{
    /* 创建或者获取共享内存 */
    key_t shm_key = ftok(program_path, 'Z');
    shmid = shmget(shm_key, SHM_SIZE, 0644 | IPC_CREAT);
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
        f->unused_nodes = clist_create();

        /* 创建根目录 */
        f->root = filesystem_node_create(nullptr, Directory, "/", nullptr);
        f->cur_dir = f->root;
        /* 初始化pwd */
        f->pwd_offset = 1;
        f->pwd[0] = '/';
        f->pwd[1] = '\0';
    }
}

bool path_is_sep(char c)
{
    return c == '/' || c == '\\';
}

/**
 * 将当前路径变为其上一级路径，并返回新路径的大小, 只处理绝对路径
 * @param path 需要变化的路径
 * @param size 当前路径大小
 * @return 变化后的路径大小
 */
size_t path_to_parent_path(char* path, size_t size)
{
    // 根目录不动
    if (size <= 1) {
        return size;
    }
    // 搜索上一个路径分隔符
    for (--size; size > 1 && !path_is_sep(path[size]); --size) {}
    return size;
}

/**
 * 拼接一级目录到原目录，只能拼接一级目录，被拼接的目录不能带有分隔符
 * @param path 原路径
 * @param size 原路径大小
 * @param new_path 新路径，以'\0'结束
 * @return 拼接后的路径大小
 */
size_t path_join_path(char* path, size_t size, const char* new_path)
{
    if (!path_is_sep(path[--size]))
        path[++size] = '/';
    for (size_t i = 0; new_path[i] != '\0'; ++i) {
        path[++size] = new_path[i];
    }
    path[++size] = '/';
    path[++size] = '\0';
    return size;
}

void filesystem_force_deinit()
{
    debug_printf("filesystem_force_deinit\n");
    if (f == nullptr)
        return;
    /* 防止后续重新分配到这块内存时被误认为已初始化 */
    f->magic_number = MAGIC_NUMBER_DEINITED;
    // todo 销毁过程中又有进程使用共享内存怎么办
    /* 反初始化共享锁 */
    pthread_rwlock_destroy(&f->rwlock);
    /* 释放共享内存 */
    // 分离共享内存
    if (shmdt(f) == -1) {
        perror("shmdt failed");
    }

    // 3. 获取共享内存ID以便删除
    if (shmid == -1) {
        perror("shmget for deletion failed");
        return;
    }

    // 4. 标记共享内存为待删除
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl IPC_RMID failed");
    }
    debug_printf("filesystem_force_deinit unlocked\n");
}

void filesystem_deinit()
{
    debug_printf("filesystem_deinit\n");
    if (f == nullptr)
        return;
    /* 防止有进程在使用 */
    pthread_rwlock_wrlock(&f->rwlock);
    pthread_rwlock_unlock(&f->rwlock);
    // todo 销毁过程中又有进程使用共享内存怎么办
    filesystem_force_deinit();
    debug_printf("filesystem_deinit unlocked\n");
}

/**
 * 处理单机目录的切换，出错会回溯
 * @param name 需要解析的路径名
 * @param ori_dir 原始目录，发生错误时恢复状态使用
 * @param ori_offset 原始pwd便宜
 * @param ori_pwd 原始pwd
 * @param arg_path 出错的参数
 * @return 是否成功
 */
bool _cd_parse_sigle_path(const char* name, FileSystemNode* ori_dir, size_t ori_offset, const char* ori_pwd,
                          const char* arg_path)
{
    if (strcmp(name, ".") == 0) {
        // 当前目录, 不变
    } else if (strcmp(name, "..") == 0) {
        // 上一级目录
        if (f->cur_dir != f->root) {
            f->pwd_offset = path_to_parent_path(f->pwd, f->pwd_offset);
            f->cur_dir = f->cur_dir->parent;
        } else {
            // 根目录的上一级不变
        }
    } else {
        // 查找是否存在该子目录
        auto subnode = filesystem_node_get_subnode(f->cur_dir, Directory, name);
        if (subnode != nullptr) {
            // 找到对应子目录
            f->pwd_offset = path_join_path(f->pwd, f->pwd_offset, name);
            f->cur_dir = subnode;
        } else {
            // 没有找到对应子目录
            f->cur_dir = ori_dir;
            f->pwd_offset = ori_offset;
            strcpy(f->pwd, ori_pwd);
            sprintf(filesystem_error, "cd error, dir \"%s\" not exist!", arg_path);
            perror(filesystem_error);
            return false;
        }
    }
    return true;
}

void cd(const char* path)
{
    debug_printf("cd: %s\n", path);
    static size_t pos;
    static char name[FILESYSTEM_NODE_NAME_SIZE];
    static char ori_pwd[FILESYSTEM_PWD_SIZE];

    pthread_rwlock_wrlock(&f->rwlock);

    // 保留当前目录，方便查找错误后的回溯
    auto ori_dir = f->cur_dir;
    auto ori_offset = f->pwd_offset;
    strcpy(ori_pwd, f->pwd);

    // 特殊处理绝对目录
    pos = 0;
    name[pos] = '\0';
    for (int i = 0; path[i] != '\0'; i++) {
        if (path_is_sep(path[i])) {
            // 处理这一级目录操作
            name[pos] = '\0';
            bool unlocked = _cd_parse_sigle_path(name, ori_dir, ori_offset, ori_pwd, path);
            if (!unlocked) {
                // 发生错误，回溯结构并报错
                break;
            }
            // 复位name
            pos = 0;
            name[pos] = '\0';
        } else {
            // 正常情况下更新name
            name[pos++] = path[i];
        }
    }
    // 处理最后一级目录
    if (pos > 0) {
        name[pos] = '\0';
        _cd_parse_sigle_path(name, ori_dir, ori_offset, ori_pwd, path);
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("cd: %s unlocked\n", name);
}

void pwd()
{
    debug_printf("pwd\n");
    pthread_rwlock_rdlock(&f->rwlock);
    printf("%s\n", f->pwd);
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("pwd unlocked\n");
}

void mkdir(const char* name)
{
    debug_printf("mkdir %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    filesystem_node_create(f->cur_dir, Directory, name, nullptr);
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("mkdir unlocked\n");
}

void rmdir(const char* name)
{
    debug_printf("rmdir %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    // 搜索node
    auto subnode = filesystem_node_get_subnode(f->cur_dir, Directory, name);
    if (subnode == nullptr) {
        sprintf(filesystem_error, "rmdir error, dir \"%s\" not exist!", name);
        perror(filesystem_error);
    } else {
        filesystem_node_destroy(subnode);
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("rmdir unlocked\n");
}

void ls()
{
    debug_printf("ls\n");
    pthread_rwlock_rdlock(&f->rwlock);
    auto subnode_list = (CList*)f->cur_dir->data;
    for (auto it = clist_begin(subnode_list); it != clist_end(subnode_list); it = clist_iterator_next(it)) {
        auto subnode = (FileSystemNode*)clist_iterator_get(it);
        if (subnode == nullptr)
            continue;
        printf("%s  type=%s\n", subnode->name, FileSystemNodeTypeNames[subnode->type]);
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("ls unlocked\n");
}

void create_file(const char* name, const char* data)
{
    debug_printf("create_file %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    // 为文件内存分配空间
    char *file_data = nullptr;
    if (data != nullptr) {
        auto file_size = strlen(data) + 4;
        file_data = (char *)alloc_memory(file_size);
        strcpy(file_data, data);
    }
    filesystem_node_create(f->cur_dir, File, name, (void*)file_data);
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("create_file unlocked\n");
}

void alter_file(const char* name, const char* data)
{
    debug_printf("alter_file %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    auto subnode = filesystem_node_get_subnode(f->cur_dir, File, name);
    if (subnode == nullptr) {
        sprintf(filesystem_error, "alter_file error, dir \"%s\" not exist!", name);
        perror(filesystem_error);
    } else {
        // todo 修改内容较短的情况下可以复用
        auto old_data = (char*)subnode->data;
        free_memory(old_data);
        // 复制新的数据到内存
        auto data_size = strlen(data) + 4;
        auto file_data = (char *)alloc_memory(data_size);
        strcpy(file_data, data);
        subnode->data = (void*)file_data;
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("alter_file unlocked\n");
}

void read_file(const char* name)
{
    debug_printf("read_file %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    auto subnode = filesystem_node_get_subnode(f->cur_dir, File, name);
    if (subnode == nullptr) {
        sprintf(filesystem_error, "read_file error, dir \"%s\" not exist!", name);
        perror(filesystem_error);
    } else {
        auto data = (char*)subnode->data;
        if (data == nullptr) {
            printf("\n");
        } else {
            printf("%s\n", data);
        }
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("read_file unlocked\n");
}

void remove_file(const char* name)
{
    debug_printf("remove_file %s\n", name);
    pthread_rwlock_wrlock(&f->rwlock);
    auto subnode = filesystem_node_get_subnode(f->cur_dir, File, name);
    if (subnode == nullptr) {
        sprintf(filesystem_error, "remove_file error, dir \"%s\" not exist!", name);
        perror(filesystem_error);
    } else {
        filesystem_node_destroy(subnode);
    }
    pthread_rwlock_unlock(&f->rwlock);
    debug_printf("remove_file unlocked\n");
}
