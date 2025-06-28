#include <stdio.h>
#include <string.h>
#include "myfilesystem.h"


int main(int argc, char* argv[])
{
    if (argc <= 1) {
        printf("Usage: 在命令行参数出入命令\n");
        return 0;
    }

    // 初始化文件系统，或者获取其共享内寸
    filesystem_init(argv[0]);

    // 解析命令行参数，每次只执行一个命令
    if (strcmp(argv[1], "cd") == 0) {
        if (argc < 3) {
            printf("cd: 请输入需要去的路径\n");
            return 0;
        }
        cd(argv[2]);
    } else if (strcmp(argv[1], "pwd") == 0) {
        pwd();
    } else if (strcmp(argv[1], "mkdir") == 0) {
        if (argc < 3) {
            printf("mkdir: 请输入需要创建的目录名\n");
            return 0;
        }
        mkdir(argv[2]);
    } else if (strcmp(argv[1], "rmdir") == 0) {
        if (argc < 3) {
            printf("mkdir: 请输入需要删除的目录名\n");
            return 0;
        }
        rmdir(argv[2]);
    } else if (strcmp(argv[1], "ls") == 0) {
        ls();
    } else if (strcmp(argv[1], "create_file") == 0) {
        if (argc < 3) {
            printf("create_file: 请输入需要创建的文件名\n");
        } else if (argc < 4) {
            create_file(argv[2], nullptr);
        } else {
            create_file(argv[2], argv[3]);
        }
    } else if (strcmp(argv[1], "alter_file") == 0) {
        if (argc < 3) {
            printf("alert_file: 请输入需要修改的文件名\n");
        } else if (argc < 4) {
            printf("alert_file: 请输入需要修改的文件内容\n");
        } else {
            alter_file(argv[2], argv[3]);
        }
    } else if (strcmp(argv[1], "read_file") == 0) {
        if (argc < 3) {
            printf("read_file: 请输入需要读取的文件名\n");
        } else {
            read_file(argv[2]);
        }
    } else if (strcmp(argv[1], "remove_file") == 0) {
        if (argc < 3) {
            printf("remove_file: 请输入需要删除的文件名\n");
        } else {
            remove_file(argv[2]);
        }
    } else if (strcmp(argv[1], "deinit") == 0) {
        filesystem_deinit();
    } else if (strcmp(argv[1], "force_deinit") == 0) {
        filesystem_force_deinit();
    } else {
        printf("参数 \"%s\" 错误\n", argv[1]);
    }

    return 0;
}
