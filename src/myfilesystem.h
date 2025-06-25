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

void filesystem_init();
void filesystem_destory();

void cd(const char *path);
void mkdir(const char *path);
void rmdir(const char *path);
void open(const char *path);
void ls(const char *path);

#endif //MYFILESYSTEM_H
