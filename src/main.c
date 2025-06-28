#include <stdio.h>
#include <string.h>
#include "myfilesystem.h"


int main(int argc, char *argv[])
{
    if (argc <= 1) {
        printf("Usage: 在命令行参数出入命令\n");
        return 0;
    }

    // 初始化文件系统，或者获取其共享内寸
    filesystem_init();

    // 解析命令行参数，每次只执行一个命令
    if (strcmp(argv[1], "cd")) {
        if (argc < 3) {
            printf("cd: 请输入需要去的路径\n");
            return 0;
        }
        cd(argv[2]);
    } else if (strcmp(argv[1], "pwd")) {
        pwd();
    } else if (strcmp(argv[1], "mkdir")) {
        if (argc < 3) {
            printf("mkdir: 请输入需要创建的目录名\n");
            return 0;
        }
    } else if (strcmp(argv[1], "rmdir")) {
        if (argc < 3) {
            printf("mkdir: 请输入需要删除的目录名\n");
            return 0;
        }
        rmdir(argv[2]);
    }

    return 0;
}
