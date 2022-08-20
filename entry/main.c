#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#include <getopt.h>
#else
#include "../msch/getopt.h"
#endif // _MSC_VER

#include "../anyf/anyf.h"
#include "../ospath/ospath.h"
#include "info.h"
#include "main.h"

int ParseCommands(int argc, char **argvs) {
    bool Overwrite = false;
    bool Append = false;
    bool Recursion = false;
    int SubOption;
    ANYF_T *pAnyfType; // [anyf]文件信息结构体指针
    static char AnyfFilePath[PATH_MAX_SIZE];
    static char TargetPath[PATH_MAX_SIZE];
    static char JPEGFilePath[PATH_MAX_SIZE];
    static char Executable[PATH_MAX_SIZE];
    static char NameToExtract[PATH_MAX_SIZE];
    const char *pNameToExtract = NameToExtract;
    // 主命令，必须是第一个命令行参数
    const char *MAINCMD_HELP = "help"; // 显示此程序的帮助信息
    const char *MAINCMD_VERS = "vers"; // 显示此程序的版本信息
    const char *MAINCMD_INFO = "info"; // 显示[anyf]文件信息及其子文件列表
    const char *MAINCMD_PACK = "pack"; // 将目录或文件打包为[anyf]文件
    const char *MAINCMD_FAKE = "fake"; // 打包目录或文件并将其伪装为JPEG文件
    const char *MAINCMD_EXTR = "extr"; // 从[anyf]文件中提取目录或文件

    const char *SUBCMD_INFO = "f:";        // 主命令[info]的子选项
    const char *SUBCMD_PACK = "f:t:ora";   // 主命令[pack]的子选项
    const char *SUBCMD_FAKE = "j:f:t:ora"; // 主命令[fake]的子选项
    const char *SUBCMD_EXTR = "f:t:n:o";   // 主命令[extr]的子选项

    if (argc < 2) {
        fprintf(stderr, MESSAGE_ERROR "命令行参数不足，请使用 %s 命令查看使用帮助。\n", MAINCMD_HELP);
        return EXIT_CODE_FAILURE;
    }
    AnyfFilePath[0] = EMPTY_CHAR;
    TargetPath[0] = EMPTY_CHAR;
    JPEGFilePath[0] = EMPTY_CHAR;
    optind = 2; // 查找参数从第3个开始，否则查不到（此变量是 getopt.h 全局变量）
    OsPathSplitExt(Executable, PATH_MAX_SIZE, NULL, 0, OsPathBaseName(NULL, 0, argvs[0]), '.');
    if (!strcmp(argvs[1], MAINCMD_HELP)) {
        printf(COMMANDUSAGE, Executable);
        return EXIT_CODE_SUCCESS;
    } else if (!strcmp(argvs[1], MAINCMD_VERS)) {
        printf(AUTHOR_INFO "\n" BUILT_INFO "\n", Executable);
        return EXIT_CODE_SUCCESS;
    } else if (!strcmp(argvs[1], MAINCMD_PACK)) {
        while ((SubOption = getopt(argc, argvs, SUBCMD_PACK)) != -1) {
            switch (SubOption) {
            case 'f':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(AnyfFilePath, optarg);
                break;
            case 't':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(TargetPath, optarg);
                break;
            case 'a':
                Append = true;
                break;
            case 'r':
                Recursion = true;
                break;
            case 'o':
                Overwrite = true;
                break;
            default:
                fprintf(stderr, MESSAGE_ERROR "没有此选项：-%c，请使用'%s %s'命令查看使用帮助。", optopt, Executable, MAINCMD_HELP);
                return EXIT_CODE_FAILURE;
            }
        }
        if (!*AnyfFilePath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入[anyf]文件路径，此路径应使用[-f]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (!*TargetPath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入要打包的目标路径，此路径应使用[-t]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (OsPathExists(AnyfFilePath))
            pAnyfType = AnyfOpen(AnyfFilePath);
        else
            pAnyfType = AnyfMake(AnyfFilePath, Overwrite);
        AnyfPack(TargetPath, Recursion, pAnyfType, Append);
        AnyfClose(pAnyfType);
        return EXIT_CODE_SUCCESS;
    } else if (!strcmp(argvs[1], MAINCMD_EXTR)) {
        while ((SubOption = getopt(argc, argvs, SUBCMD_EXTR)) != -1) {
            switch (SubOption) {
            case 'n':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "输入的文件名过长\n");
                    return EXIT_CODE_FAILURE;
                }
                strcpy(NameToExtract, optarg);
                break;
            case 'f':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(AnyfFilePath, optarg);
                break;
            case 't':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(TargetPath, optarg);
                break;
            case 'o':
                Overwrite = true;
                break;
            default:
                fprintf(stderr, MESSAGE_ERROR "没有此选项：-%c，请使用'%s %s'命令查看使用帮助。", optopt, Executable, MAINCMD_HELP);
                return EXIT_CODE_FAILURE;
            }
        }
        if (!*AnyfFilePath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入[anyf]文件路径，此路径应使用[-f]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (!*TargetPath)
            strcpy(TargetPath, PATH_CDIRS);
        if (!*NameToExtract)
            pNameToExtract = NULL;
        if (AnyfIsFakeJPEG(AnyfFilePath))
            pAnyfType = AnyfOpenFakeJPEG(AnyfFilePath);
        else
            pAnyfType = AnyfOpen(AnyfFilePath);
        AnyfExtract(pNameToExtract, TargetPath, Overwrite, pAnyfType);
        AnyfClose(pAnyfType);
        return EXIT_CODE_SUCCESS;
    } else if (!strcmp(argvs[1], MAINCMD_INFO)) {
        while ((SubOption = getopt(argc, argvs, SUBCMD_INFO)) != -1) {
            switch (SubOption) {
            case 'f':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(AnyfFilePath, optarg);
                break;
            default:
                fprintf(stderr, MESSAGE_ERROR "没有此选项：-%c，请使用'%s %s'命令查看使用帮助。", optopt, Executable, MAINCMD_HELP);
                return EXIT_CODE_FAILURE;
            }
        }
        if (!*AnyfFilePath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入[anyf]文件路径，此路径应使用[-f]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        pAnyfType = AnyfInfo(AnyfFilePath);
        AnyfClose(pAnyfType);
        return EXIT_CODE_SUCCESS;
    } else if (!strcmp(argvs[1], MAINCMD_FAKE)) {
        while ((SubOption = getopt(argc, argvs, SUBCMD_FAKE)) != -1) {
            switch (SubOption) {
            case 'f':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(AnyfFilePath, optarg);
                break;
            case 't':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(TargetPath, optarg);
                break;
            case 'a':
                Append = true;
                break;
            case 'r':
                Recursion = true;
                break;
            case 'o':
                Overwrite = true;
                break;
            case 'j':
                if (strlen(optarg) >= PATH_MAX_SIZE) {
                    fprintf(stderr, MESSAGE_ERROR "路径太长：%s\n", optarg);
                    return EXIT_CODE_FAILURE;
                }
                strcpy(JPEGFilePath, optarg);
                break;
            default:
                fprintf(stderr, MESSAGE_ERROR "没有此选项：-%c，请使用'%s %s'命令查看使用帮助。", optopt, Executable, MAINCMD_HELP);
                return EXIT_CODE_FAILURE;
            }
        }
        if (!*AnyfFilePath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入[anyf]文件路径，此路径应使用[-f]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (!*TargetPath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入要打包的目标路径，此路径应使用[-t]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (!*JPEGFilePath) {
            fprintf(stderr, MESSAGE_ERROR "没有输入[JPEG]文件路径，此路径应使用[-j]选项指定\n");
            return EXIT_CODE_FAILURE;
        }
        if (OsPathExists(AnyfFilePath)) {
            printf(MESSAGE_WARN "已存在[anyf]文件，[-j]及[-o]选项将不生效。\n");
            pAnyfType = AnyfOpenFakeJPEG(AnyfFilePath);
        } else {
            pAnyfType = AnyfMakeFakeJPEG(AnyfFilePath, JPEGFilePath, Overwrite);
        }
        AnyfPack(TargetPath, Recursion, pAnyfType, Append);
        AnyfClose(pAnyfType);
        return EXIT_CODE_SUCCESS;
    } else {
        fprintf(stderr, MESSAGE_ERROR "没有此命令：%s，请使用'%s %s'命令查看使用帮助。\n", argvs[1], Executable, MAINCMD_HELP);
        return EXIT_CODE_FAILURE;
    }
    return EXIT_CODE_SUCCESS;
}

int main(int argc, char *argvs[]) {
// DEBUG 宏 PACK_DEBUG 定义位置：
//      CMAKE 工程：定义在'./CMakeLists.txt'中
//      VS 工程：定义在'属性管理器->msbuild->Debug'中
#ifdef PACK_DEBUG
    printf(MESSAGE_WARN "调试：请更改'entry->main.c->main'函数的调试参数\n");
    argc = 6;
    char *cust[256];
    // ./anyf pack -f ./1.af -t .
    cust[0] = "./anyf";
    cust[1] = "pack";
    cust[2] = "-f";
    cust[3] = "./1.af";
    cust[4] = "-t";
    cust[5] = ".";
    // cust[6] = "-j";
    // cust[7] = "1.jpeg";
    return ParseCommands(argc, cust);
#else
    return ParseCommands(argc, argvs);
#endif
}
