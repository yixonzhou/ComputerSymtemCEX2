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

// 为了防止递归加锁，这里加锁只在暴露的api的最外层加锁
void filesystem_init();
void filesystem_deinit();

void cd(const char *path);
void pwd();
void mkdir(const char *name);
void rmdir(const char *name);
void ls();
void open(const char *name, const char *data);
void alert(const char *name, const char *data);
void rm(const char *name);

#endif //MYFILESYSTEM_H
