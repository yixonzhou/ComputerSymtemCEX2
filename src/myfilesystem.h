/**
  ******************************************************************************
  * @encoding  utf-8
  * @file      myfilesystem.h
  * @author    ZYX
  * @brief     None
  ******************************************************************************
  */

#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H

/**
 *
 */
typedef struct FileSystem FileSystem;

int debug_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));

// 为了防止递归加锁，这里加锁只在暴露的api的最外层加锁
void filesystem_init(const char *program_path);
void filesystem_deinit();
void filesystem_force_deinit();

void cd(const char *path);
void pwd();
void mkdir(const char *name);
void rmdir(const char *name);
void ls();
void create_file(const char *name, const char *data);
void alter_file(const char *name, const char *data);
void read_file(const char *name);
void remove_file(const char *name);


#endif //MYFILESYSTEM_H
