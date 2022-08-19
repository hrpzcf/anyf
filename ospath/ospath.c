#include "ospath.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <shlwapi.h>
#include <windows.h>
#pragma comment(lib, "shlwapi.lib")
#define OSP_AFS "*"
#else
#include <dirent.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif // _WIN32
#endif // _MSC_VER

#define EXCLUDE_RECS "$RECYCLE.BIN"
#define EXCLUDE_SVIS "System Volume Information"

// 分配内存时一次重新分配块的数量
// 不一定是内存大小，只是广义上的数量
#define MALLOC_NUM 128
#define RALLOC_NUM 128

// 最后一次函数执行状态码
static int OSP_LAST_STATE = STATUS_EXEC_SUCCESS;

// 设置函数的完成状态
static inline void OsPathSetState(int state) { OSP_LAST_STATE = state; }

// 获取最后一次函数执行的错误代码
// 返回0代表没有发生错误
// 此函数返回值见同名头文件中名称以<STATUS_>开头的宏定义
int OsPathLastState(void) { return OSP_LAST_STATE; }

// 将字符串转换为全小写，改变原字符串
static char *StrToLowercase(char *String) {
    char *Temp = String;
    for (; *Temp; ++Temp)
        *Temp = tolower(*Temp);
    return String;
}

// 验证路径是否存在
bool OsPathExists(const char *Path) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
#ifndef _MSC_VER
    return !access(Path, F_OK);
#else
    return PathFileExistsA(Path);
#endif // _MSC_VER
}

// 验证路径是否是一个目录
bool OsPathIsDirectory(const char *Path) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
#ifndef _MSC_VER
    struct stat Buffer;
    if (stat(Path, &Buffer)) {
        OsPathSetState(STATUS_GET_ATTR_FAIL);
        return RESULT_FALSE;
    }
    return S_ISDIR(Buffer.st_mode);
#else
    DWORD fattr = GetFileAttributesA(Path);
    if (fattr == INVALID_FILE_ATTRIBUTES)
        return RESULT_FALSE;
    return (fattr & FILE_ATTRIBUTE_DIRECTORY);
#endif // _MSC_VER
}

// 验证路径是否是一个文件
bool OsPathIsFile(const char *Path) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
#ifndef _MSC_VER
    struct stat Buffer;
    if (stat(Path, &Buffer)) {
        OsPathSetState(STATUS_GET_ATTR_FAIL);
        return RESULT_FALSE;
    }
    return !S_ISDIR(Buffer.st_mode);
#else
    DWORD fattr = GetFileAttributesA(Path);
    if (fattr == INVALID_FILE_ATTRIBUTES)
        return RESULT_FALSE;
    return !(fattr & FILE_ATTRIBUTE_DIRECTORY);
#endif // _MSC_VER
}

// 创建多级目录
// 成功返回0，失败返回1
int OsPathMakeDIR(const char *DirPath) {
    static char Buffer[PATH_MSIZE + 12];
    char *CmdPrefix = "mkdir";
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!DirPath || strlen(DirPath) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
#ifdef _WIN32
    sprintf(Buffer, "%s \"%s\"", CmdPrefix, DirPath);
#else
    sprintf(Buffer, "%s -p \"%s\"", CmdPrefix, DirPath);
#endif
    if (system(Buffer)) {
        OsPathSetState(STATUS_COMMAND_FAIL);
        return RESULT_FAILURE;
    } else
        return RESULT_SUCCESS;
}

// 获取当前工作目录
// 参数buf是接收路径的缓冲区
// 参数size是缓冲区大小
// 如果参数buf为NULL，则应注意，之前引用get_cwd(NULL, 0)返回值
// 地址的变量的值都有可能被此次运行改变
char *OsPathGetCWD(char *Buffer, size_t Size) {
    static char CWD[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (NULL == Buffer) {
        Buffer = CWD;
        Size = PATH_MSIZE;
    }
#ifndef _MSC_VER
#ifdef _WIN32
    return _getcwd(Buffer, Size);
#else
    return getcwd(Buffer, Size);
#endif // _WIN32
#else
    return GetCurrentDirectoryA((DWORD)Size, Buffer) ? Buffer : NULL;
#endif // _MSC_VER
}

// 创建scanlist_t结构体并分配内存，返回结构体指针
// 如果参数blocks为0则默认以MALLOC_NUM代替,此处指定的blocks仅指定初始空间数量
// 后续enrich_scanlist收集路径时如果内存不足会自动扩充内存
SCANNER_T *OsPathMakeScanner(size_t Blocks) {
    SCANNER_T *pScanner;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (Blocks <= 0)
        Blocks = MALLOC_NUM;
    pScanner = malloc(Blocks * sizeof(SCANNER_T) + sizeof(char *));
    if (!pScanner) {
        OsPathSetState(STATUS_MEMORY_ERROR);
        return NULL;
    }
    pScanner->count = 0, pScanner->blocks = Blocks;
    return pScanner;
}

// 关闭scanlist_t对象，释放内存
int OsPathDeleteScanner(SCANNER_T *Scanner) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (Scanner) {
        for (size_t i = 0; i < Scanner->count; ++i) {
            free(Scanner->paths[i]);
        }
        free(Scanner), Scanner = NULL;
    }
    return RESULT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER

// 功能：搜索给定路径中的文件或目录
// 参数 ppScanner 为结构体scanlist_t指针的指针
// 参数 DirPath 为要搜索的目录路径
// 参数 Recursion
// 为要收集的路径类型，可用值：FTYPE_DIR为目录，FTYPE_FILE为文件，FTYPE_BOTH则两者兼顾 参数
// Recursion 控制是否递归搜索子目录
int OsPathScanPath(const char *DirPath, int Target, int Recursion, SCANNER_T **const ppScanner) {
    size_t DirPathLength, NumOfBytesToMalloc;
    SCANNER_T *pScannerTemp; // 仅为了让Visual Studio不显示警告
    char *pFullPathToEachFile = NULL;
    char pPathToFindFile[PATH_MSIZE];
    int FinalReturnCode = RESULT_SUCCESS;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (NULL == ppScanner || NULL == *ppScanner) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    pScannerTemp = *ppScanner;
    if (!OsPathIsDirectory(DirPath)) {
        if (!OsPathLastState())
            OsPathSetState(STATUS_INVALID_PARAM);
        return RESULT_FAILURE;
    }
    DirPathLength = strlen(DirPath);
    if (DirPathLength >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(pPathToFindFile, DirPath);
    if (OsPathJoinPath(pPathToFindFile, PATH_MSIZE, 2, pPathToFindFile, OSP_AFS))
        return RESULT_FAILURE;
    WIN32_FIND_DATAA StructWinFindData;
    HANDLE HandleOfFindFile;
    if (INVALID_HANDLE_VALUE == (HandleOfFindFile = FindFirstFileA(pPathToFindFile, &StructWinFindData))) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_COMMAND_FAIL);
        goto CloseAndReturn;
    }
    // 直接FindNextFileA仍然可以获得FindFirstFileA的结果
    while (0 != FindNextFileA(HandleOfFindFile, &StructWinFindData)) {
        if (strcmp(StructWinFindData.cFileName, PATH_CDIRS) == 0 || strcmp(StructWinFindData.cFileName, PATH_PDIRS) == 0 || strcmp(StructWinFindData.cFileName, EXCLUDE_RECS) == 0 || strcmp(StructWinFindData.cFileName, EXCLUDE_SVIS) == 0)
            continue;
        if ((*ppScanner)->count >= (*ppScanner)->blocks) {
            // 用sizeof(*ppScanner)得不到原对象已分配内存大小
            pScannerTemp = realloc(*ppScanner, sizeof(SCANNER_T) + sizeof(char *) * ((*ppScanner)->blocks + RALLOC_NUM));
            if (NULL == pScannerTemp) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_MEMORY_ERROR);
                goto CloseAndReturn;
            }
            *ppScanner = pScannerTemp;
            (*ppScanner)->blocks += RALLOC_NUM;
        }
        // 为cFileName及dir_path、PATH_NSEPS、末尾0分配空间
        NumOfBytesToMalloc = DirPathLength + strlen(StructWinFindData.cFileName) + 2;
        pFullPathToEachFile = malloc(NumOfBytesToMalloc);
        if (NULL == pFullPathToEachFile) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_MEMORY_ERROR);
            goto CloseAndReturn;
        }
        if (OsPathJoinPath(pFullPathToEachFile, NumOfBytesToMalloc, 2, DirPath, StructWinFindData.cFileName)) {
            free(pFullPathToEachFile);
            continue;
        }
        if (!OsPathNormpath(pFullPathToEachFile, NumOfBytesToMalloc)) {
            free(pFullPathToEachFile);
            continue;
        }
        if (StructWinFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (Target & OSPATH_BOTH || Target & OSPATH_DIR)
                (*ppScanner)->paths[(*ppScanner)->count++] = pFullPathToEachFile;
            if (Recursion)
                FinalReturnCode = OsPathScanPath(pFullPathToEachFile, Target, Recursion, ppScanner);
            if (!(Target & OSPATH_BOTH) && !(Target & OSPATH_DIR))
                if (NULL != pFullPathToEachFile)
                    free(pFullPathToEachFile), pFullPathToEachFile = NULL;
            if (FinalReturnCode == RESULT_FAILURE)
                goto CloseAndReturn;
        } else {
            if (Target & OSPATH_BOTH || Target & OSPATH_FILE)
                (*ppScanner)->paths[(*ppScanner)->count++] = pFullPathToEachFile;
            else if (NULL != pFullPathToEachFile)
                free(pFullPathToEachFile), pFullPathToEachFile = NULL;
        }
    }
CloseAndReturn:
    FindClose(HandleOfFindFile);
    return FinalReturnCode;
}

#else // __GNUC__

// 功能：搜索给定路径中的文件或目录
// 参数 ppScanner 为结构体scanlist_t指针的指针
// 参数 DirPath 为要搜索的目录路径
// 参数 Recursion
// 为要收集的路径类型，可用值：FTYPE_DIR为目录，FTYPE_FILE为文件，FTYPE_BOTH则两者兼顾 参数
// Recursion 控制是否递归搜索子目录
int OsPathScanPath(const char *DirPath, int Target, int Recursion, SCANNER_T **const ppScanner) {
    size_t DirPathLength, NumOfBytesToMalloc;
    char *pFullPathToEachFile = NULL;
    int FinalReturnCode = RESULT_SUCCESS;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (NULL == ppScanner || NULL == *ppScanner) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    DirPathLength = strlen(DirPath);
    if (!OsPathIsDirectory(DirPath)) {
        if (!OsPathLastState())
            OsPathSetState(STATUS_INVALID_PARAM);
        return RESULT_FAILURE;
    }
    // MinGW 的 dirent 没有 d_type，所以统一使用 stat
    struct stat StatBuffer;
    struct dirent *PathDirent;
    DIR *OpenedDir;
    if (!(OpenedDir = opendir(DirPath))) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_COMMAND_FAIL);
        goto CloseAndReturn;
    }
    while (NULL != (PathDirent = readdir(OpenedDir))) {
        if (strcmp(PathDirent->d_name, PATH_CDIRS) == 0 || strcmp(PathDirent->d_name, PATH_PDIRS) == 0 || strcmp(PathDirent->d_name, EXCLUDE_RECS) == 0 || strcmp(PathDirent->d_name, EXCLUDE_SVIS) == 0)
            continue;
        if ((*ppScanner)->count >= (*ppScanner)->blocks) {
            // 用sizeof(*ppScanner)得不到原对象已分配内存大小
            *ppScanner = realloc(*ppScanner, sizeof(SCANNER_T) + sizeof(char *) * ((*ppScanner)->blocks + RALLOC_NUM));
            if (NULL == *ppScanner) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_MEMORY_ERROR);
                goto CloseAndReturn;
            }
            (*ppScanner)->blocks += RALLOC_NUM;
        }
        // 为dname和dir_path及PATH_NSEPS、末尾0分配空间
        NumOfBytesToMalloc = DirPathLength + strlen(PathDirent->d_name) + 2;
        // 先拼接buf_full_path再用于stat
        pFullPathToEachFile = malloc(NumOfBytesToMalloc);
        if (NULL == pFullPathToEachFile) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_MEMORY_ERROR);
            goto CloseAndReturn;
        }
        if (OsPathJoinPath(pFullPathToEachFile, NumOfBytesToMalloc, 2, DirPath, PathDirent->d_name)) {
            free(pFullPathToEachFile);
            continue;
        }
        if (!OsPathNormpath(pFullPathToEachFile, NumOfBytesToMalloc)) {
            free(pFullPathToEachFile);
            continue;
        }
        if (stat(pFullPathToEachFile, &StatBuffer))
            continue;
        if (S_ISDIR(StatBuffer.st_mode)) {
            if (Target & OSPATH_BOTH || Target & OSPATH_DIR)
                (*ppScanner)->paths[(*ppScanner)->count++] = pFullPathToEachFile;
            if (Recursion)
                FinalReturnCode = OsPathScanPath(pFullPathToEachFile, Target, Recursion, ppScanner);
            if (Target != OSPATH_BOTH && Target != OSPATH_DIR)
                if (pFullPathToEachFile)
                    free(pFullPathToEachFile), pFullPathToEachFile = NULL;
            if (FinalReturnCode == RESULT_FAILURE)
                goto CloseAndReturn;
        } else {
            if (Target & OSPATH_BOTH || Target & OSPATH_FILE)
                (*ppScanner)->paths[(*ppScanner)->count++] = pFullPathToEachFile;
            else if (NULL != pFullPathToEachFile)
                free(pFullPathToEachFile), pFullPathToEachFile = NULL;
        }
    }
CloseAndReturn:
    closedir(OpenedDir);
    return FinalReturnCode;
}

#endif // _MSC_VER
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32 // 区分win32及posix平台

// 验证路径是否是绝对路径
// 是：返回1；否：返回0
// 当函数返回0时，最好获取path_last_error函数返回值，并验证返回值是否是STATUS_EXEC_SUCCESS
// 如果返回值不是STATUS_EXEC_SUCCESS，那么表示is_abs是因出错才返回0，结果将是不可靠的
bool OsPathIsAbsolute(const char *Path) {
    char PathBufferTemp[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FALSE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FALSE;
    }
    strcpy(PathBufferTemp, Path);
    if (!OsPathNormcase(PathBufferTemp))
        return RESULT_FALSE;
    else if (!strncmp(PathBufferTemp, PATH_UNCQS, 4)) {
        OsPathSetState(STATUS_OTHER_ERRORS);
        return RESULT_TRUE;
    } else if (OsPathSplitDrive(NULL, 0, PathBufferTemp, PATH_MSIZE, PathBufferTemp))
        return RESULT_FALSE;
    return (strlen(PathBufferTemp) > 0 && *PathBufferTemp == PATH_NSEP);
}

// 将路径中的斜杠转换为系统标准形式，大写转小写
// 此函数改变原路径，成功返回缓冲区指针，失败返回NULL
char *OsPathNormcase(char *Path) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return NULL;
    }
    size_t Length = strlen(Path);
    if (Length < PATH_MSIZE) {
        for (size_t i = 0; i < Length; ++i) {
            if (Path[i] == PATH_ASEP)
                Path[i] = PATH_NSEP;
            else
                Path[i] = tolower(Path[i]);
        }
        return Path;
    } else {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return NULL;
    }
}

// 1.将路径中的斜杠转换为系统标准形式
// 2.将路径中的可定位的相对路径字符'.'和'..'进行重定位
// 3.参数size为字符串数组path的空间大小(有可能比字符串path本身长的多)
// 4.改变path的时候，字符串长度可能会变，多数时候不变或变短，但空字符串会变为1字符
//    所以，就算path字符串长度为0，也要保证字符串数组path内存空间大于等于2
// 此函数改变原路径，成功返回缓冲区指针，失败返回NULL
char *OsPathNormpath(char *Path, size_t Size) {
    size_t PathLength, Index = 0, SizeRequired = 0;
    char *FinalResult = Path;
    char *pPath = Path;
    char PathTempArray[PATH_MSIZE];
    char **ppSplitedPath;
    char *SplitToken, *Prefix, *Suffix;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return NULL;
    }
    PathLength = strlen(Path);
    if (PathLength >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return NULL;
    }
    if ((PathLength >= 4) && (!strncmp(Path, PATH_UNCDS, 4) || !strncmp(Path, PATH_UNCQS, 4)))
        return FinalResult;
    Prefix = malloc(PATH_MSIZE);
    Suffix = malloc(PATH_MSIZE);
    ppSplitedPath = malloc(PATH_MSIZE * sizeof(char *));
    if (!ppSplitedPath || !Prefix || !Suffix) {
        FinalResult = NULL;
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    while (*pPath) {
        if (*pPath == PATH_ASEP)
            *pPath = PATH_NSEP;
        ++pPath;
    }
    if (OsPathSplitDrive(Prefix, PATH_MSIZE, Suffix, PATH_MSIZE, Path)) {
        FinalResult = NULL;
        goto CleanAndReturn;
    }
    if (Suffix[0] == PATH_NSEP) {
        strcat(Prefix, PATH_NSEPS);
        memmove(Suffix, Suffix + 1, strlen(Suffix));
    }
    SplitToken = strtok(Suffix, PATH_NSEPS);
    while (SplitToken) {
        if (strcmp(SplitToken, PATH_CDIRS) == 0)
            goto Next;
        else if (strcmp(SplitToken, PATH_PDIRS) == 0) {
            if (Index > 0 && strcmp(ppSplitedPath[Index - 1], PATH_PDIRS))
                --Index;
            else if (Index == 0 && Prefix[2] == PATH_NSEP)
                goto Next;
            else
                ppSplitedPath[Index++] = SplitToken;
        } else
            ppSplitedPath[Index++] = SplitToken;
    Next:
        SplitToken = strtok(NULL, PATH_NSEPS);
    }
    if (!*Prefix && Index == 0) {
        if (Size <= 2) {
            FinalResult = NULL;
            OsPathSetState(STATUS_INSFC_BUFFER);
            goto CleanAndReturn;
        }
        strcpy(Path, PATH_CDIRS);
        goto CleanAndReturn;
    } else {
        if (Size <= strlen(Prefix)) {
            FinalResult = NULL;
            OsPathSetState(STATUS_INSFC_BUFFER);
            goto CleanAndReturn;
        }
        SizeRequired += strlen(Prefix);
        strcpy(PathTempArray, Prefix);
        for (size_t i = 0; i < Index; ++i) {
            SizeRequired += strlen(ppSplitedPath[i]);
            if (Size <= SizeRequired) {
                FinalResult = NULL;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto CleanAndReturn;
            }
            strcat(PathTempArray, ppSplitedPath[i]);
            if (i != Index - 1)
                strcat(PathTempArray, PATH_NSEPS);
        }
        strcpy(Path, PathTempArray);
    }
CleanAndReturn:
    if (NULL != Prefix)
        free(Prefix);
    if (NULL != Suffix)
        free(Suffix);
    if (NULL != ppSplitedPath)
        free(ppSplitedPath);
    return FinalResult;
}

// 将路径名拆分为(驱动器/共享点)和相对路径说明符。
//      得到的路径可能是空字符串。
//
//      如果路径包含驱动器号，则 buf_d将包含所有内容直到并包括冒号。
//      例如 OsPathSplitDrive(DriveBuffer, 10, PathBuffer, 10, "c:\\dir")，buf_d被设置为
//      "c:"， buf_p被设置为"\\dir"
//
//      如果路径包含共享点路径，则buf_d将包含主机名并共享最多但不包括
//      第四个目录分隔符。例如 OsPathSplitDrive(DriveBuffer, 10, PathBuffer, 10,
//      "//host/computer/dir")
//      buf_d被设置为"//host/computer"，buf_p被设置为"/dir"。
//
//      路径不能同时包含驱动器号和共享点路径。
// 成功返回0，失败返回1
int OsPathSplitDrive(char DriveBuffer[], size_t DriveBufSize, char PathBuffer[], size_t PathBufferSize, const char *Path) {
    size_t PathLength;
    char *pSep3Index, *pSep4Index;
    char PathBufferTemp[PATH_MSIZE], Normcased[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    PathLength = strlen(Path);
    if (PathLength >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(PathBufferTemp, Path);
    if (PathLength >= 2) {
        strcpy(Normcased, PathBufferTemp);
        if (!OsPathNormcase(Normcased))
            return RESULT_FAILURE;
        if (!strncmp(Normcased, PATH_NSEPS2, 2) && Normcased[2] != PATH_NSEP) {
            pSep3Index = strchr(Normcased + 2, PATH_NSEP);
            if (NULL == pSep3Index || ((pSep4Index = strchr(pSep3Index + 1, PATH_NSEP)) == pSep3Index + 1)) {
                if (NULL != DriveBuffer) {
                    if (DriveBufSize < 1) {
                        OsPathSetState(STATUS_INSFC_BUFFER);
                        return RESULT_FAILURE;
                    }
                    DriveBuffer[0] = EMPTY_CHAR;
                }
                if (NULL != PathBuffer) {
                    if (PathBufferSize < PathLength + 1) {
                        OsPathSetState(STATUS_INSFC_BUFFER);
                        return RESULT_FAILURE;
                    }
                    strcpy(PathBuffer, PathBufferTemp);
                }
                return RESULT_SUCCESS;
            }
            // 这一句判断语句不一定执行
            // 所以后面所有sep4_index - p_cased都不能简化为p_len
            if (NULL == pSep4Index)
                pSep4Index = Normcased + PathLength;
            if (NULL != DriveBuffer) {
                if (DriveBufSize <= (size_t)(pSep4Index - Normcased)) {
                    OsPathSetState(STATUS_INSFC_BUFFER);
                    return RESULT_FAILURE;
                }
                strncpy(DriveBuffer, PathBufferTemp, pSep4Index - Normcased);
                DriveBuffer[pSep4Index - Normcased] = EMPTY_CHAR;
            }
            if (NULL != PathBuffer) {
                if (PathBufferSize <= strlen(pSep4Index)) {
                    OsPathSetState(STATUS_INSFC_BUFFER);
                    return RESULT_FAILURE;
                }
                strcpy(PathBuffer, PathBufferTemp + (pSep4Index - Normcased));
            }
            return RESULT_SUCCESS;
        }
        if (PathBufferTemp[1] == PATH_COLON) {
            if (NULL != DriveBuffer) {
                if (DriveBufSize < 3) {
                    OsPathSetState(STATUS_INSFC_BUFFER);
                    return RESULT_FAILURE;
                }
                strncpy(DriveBuffer, PathBufferTemp, 2);
                DriveBuffer[2] = EMPTY_CHAR;
            }
            if (NULL != PathBuffer) {
                if (PathBufferSize < PathLength - 1) {
                    OsPathSetState(STATUS_INSFC_BUFFER);
                    return RESULT_FAILURE;
                }
                strcpy(PathBuffer, PathBufferTemp + 2);
            }
            return RESULT_SUCCESS;
        }
    }
    if (NULL != DriveBuffer) {
        if (DriveBufSize < 1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        DriveBuffer[0] = EMPTY_CHAR;
    }
    if (NULL != PathBuffer) {
        if (PathBufferSize <= PathLength) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(PathBuffer, PathBufferTemp);
    }
    return RESULT_SUCCESS;
}

// 用系统标准路径分隔符连接第4个及其后的所有参数值
// 参数n指示了它后面参数的个数
// 注意，要被拼接的路径不应以斜杠开头，否则会覆盖前面除驱动器号以外的路径
// 成功返回0，失败返回1
int OsPathJoinPath(char Buffer[], size_t BufferSize, int NumOfParam, const char *Path, ...) {
    int FinalReturnCode = RESULT_SUCCESS;
    size_t SizeRequired = 0;
    char *Driver, *PathExceptDriver, *ResultDrive, *ResultPath, *EachArgument;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    SizeRequired += strlen(Path);
    if (SizeRequired >= PATH_MSIZE) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        return RESULT_FAILURE;
    }
    Driver = malloc(PATH_MSIZE);
    PathExceptDriver = malloc(PATH_MSIZE);
    ResultDrive = malloc(PATH_MSIZE);
    ResultPath = malloc(PATH_MSIZE);
    if (!Driver || !PathExceptDriver || !ResultDrive || !ResultPath) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    va_list ArgumentList;
    if (OsPathSplitDrive(ResultDrive, PATH_MSIZE, ResultPath, PATH_MSIZE, Path)) {
        FinalReturnCode = RESULT_FAILURE;
        goto CleanAndReturn;
    }
    va_start(ArgumentList, Path);
    for (int i = 0; i < NumOfParam - 1; ++i) {
        EachArgument = va_arg(ArgumentList, char *);
        if (OsPathSplitDrive(Driver, PATH_MSIZE, PathExceptDriver, PATH_MSIZE, EachArgument)) {
            FinalReturnCode = RESULT_FAILURE;
            goto CleanAndReturn;
        }
        if (*PathExceptDriver && (PathExceptDriver[0] == PATH_NSEP || PathExceptDriver[0] == PATH_ASEP)) {
            if (*Driver || !*ResultDrive)
                strcpy(ResultDrive, Driver);
            SizeRequired = strlen(PathExceptDriver);
            if (SizeRequired >= PATH_MSIZE) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto CleanAndReturn;
            }
            strcpy(ResultPath, PathExceptDriver);
            continue;
        } else if (*Driver && strcmp(Driver, ResultDrive)) {
            if (strcmp(StrToLowercase(Driver), StrToLowercase(ResultDrive))) {
                SizeRequired = strlen(PathExceptDriver);
                if (SizeRequired >= PATH_MSIZE) {
                    FinalReturnCode = RESULT_FAILURE;
                    OsPathSetState(STATUS_INSFC_BUFFER);
                    goto CleanAndReturn;
                }
                strcpy(ResultDrive, Driver);
                strcpy(ResultPath, PathExceptDriver);
                continue;
            }
            strcpy(ResultDrive, Driver);
        }
        SizeRequired += strlen(PathExceptDriver);
        if (SizeRequired >= PATH_MSIZE) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_INSFC_BUFFER);
            goto CleanAndReturn;
        }
        char ResultPathLastChar = ResultPath[strlen(ResultPath) - 1];
        if (*ResultPath && ResultPathLastChar != PATH_NSEP && ResultPathLastChar != PATH_ASEP) {
            ++SizeRequired;
            if (SizeRequired >= PATH_MSIZE) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto CleanAndReturn;
            }
            strcat(ResultPath, PATH_NSEPS);
        }
        strcat(ResultPath, PathExceptDriver);
    }
    va_end(ArgumentList);
    if (BufferSize <= strlen(ResultDrive) + strlen(ResultPath)) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_INSFC_BUFFER);
        goto CleanAndReturn;
    }
    if (*ResultPath && *ResultPath != PATH_NSEP && *ResultPath != PATH_ASEP && *ResultDrive && ResultDrive[strlen(ResultDrive) - 1] != PATH_COLON) {
        sprintf(Buffer, "%s%s%s", ResultDrive, PATH_NSEPS, ResultPath);
        goto CleanAndReturn;
    }
    sprintf(Buffer, "%s%s", ResultDrive, ResultPath);
CleanAndReturn:
    if (Driver)
        free(Driver);
    if (PathExceptDriver)
        free(PathExceptDriver);
    if (ResultDrive)
        free(ResultDrive);
    if (ResultPath)
        free(ResultPath);
    return FinalReturnCode;
}

// 将路径按最后一个路径分隔符(斜杠)分割成两部分
// 成功返回0，失败返回1
int OsPathSplitPath(char HeadBuffer[], size_t HeadBufSize, char TailBuffer[], size_t TailBufSize, const char *Path) {
    int Index, FinalReturnCode = RESULT_SUCCESS;
    size_t PathLength, IndexOfLastSepPlus1;
    char *FrontOfLastSep = NULL; // 右边第一个路径分隔符以左的字符串
    char Driver[PATH_MSIZE], PathExceptDriver[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    if (OsPathSplitDrive(Driver, PATH_MSIZE, PathExceptDriver, PATH_MSIZE, Path))
        return RESULT_FAILURE;
    PathLength = IndexOfLastSepPlus1 = strlen(PathExceptDriver);
    // IndexOfLastSepPlus1：去除驱动器号后的路径的最后一个斜杠下标 +1 位置
    while (IndexOfLastSepPlus1 && PathExceptDriver[IndexOfLastSepPlus1 - 1] != PATH_ASEP && PathExceptDriver[IndexOfLastSepPlus1 - 1] != PATH_NSEP)
        --IndexOfLastSepPlus1;
    if (NULL != TailBuffer) {
        if (TailBufSize <= PathLength - IndexOfLastSepPlus1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(TailBuffer, PathExceptDriver + IndexOfLastSepPlus1);
    }
    if (IndexOfLastSepPlus1 == 0)
        FrontOfLastSep = calloc(1, 1);
    else
        FrontOfLastSep = calloc(IndexOfLastSepPlus1 + 1, 1);
    if (!FrontOfLastSep) {
        OsPathSetState(STATUS_MEMORY_ERROR);
        return RESULT_FAILURE;
    }
    if (IndexOfLastSepPlus1 > 0) {
        strncpy(FrontOfLastSep, PathExceptDriver, IndexOfLastSepPlus1);
    }
    FrontOfLastSep[IndexOfLastSepPlus1] = EMPTY_CHAR;
    for (Index = (int)(IndexOfLastSepPlus1 - 1); Index >= 0; --Index) {
        if (FrontOfLastSep[Index] == PATH_ASEP || FrontOfLastSep[Index] == PATH_NSEP)
            continue;
        else {
            FrontOfLastSep[Index + 1] = EMPTY_CHAR; // 去除尾随斜杠
            break;
        }
    }
    if (NULL != HeadBuffer) {
        if (IndexOfLastSepPlus1 == 0) {
            if (HeadBufSize <= strlen(Driver)) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto CleanAndReturn;
            }
            strcpy(HeadBuffer, Driver);
            goto CleanAndReturn;
        } else {
            if (HeadBufSize <= strlen(Driver) + IndexOfLastSepPlus1) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto CleanAndReturn;
            }
            strcpy(HeadBuffer, Driver);
            strcat(HeadBuffer, FrontOfLastSep);
            goto CleanAndReturn;
        }
    }
CleanAndReturn:
    if (NULL != FrontOfLastSep)
        free(FrontOfLastSep);
    return FinalReturnCode;
}

// 获取路径的上一级路径
// 成功返回字符指针，失败返回NULL
char *OsPathDirName(char Buffer[], size_t BufferSize, const char *Path) {
    static char DirectoryPath[PATH_MSIZE];
    if (!Buffer) {
        Buffer = DirectoryPath;
        BufferSize = PATH_MSIZE;
    }
    return OsPathSplitPath(Buffer, BufferSize, NULL, 0, Path) ? NULL : Buffer;
}

// 获取路径中的文件名
// 成功返回字符指针，失败返回NULL
char *OsPathBaseName(char Buffer[], size_t BufferSize, const char *Path) {
    static char BaseName[PATH_MSIZE];
    if (!Buffer) {
        Buffer = BaseName;
        BufferSize = PATH_MSIZE;
    }
    return OsPathSplitPath(NULL, 0, Buffer, BufferSize, Path) ? NULL : Buffer;
}

// 功能：生成相对路径
// 此相对路径是 Path1 相对于 Path2 的路径
// 成功返回0，失败返回1
int OsPathRelativePath(char Buffer[], size_t BufferSize, const char *Path1, const char *Path2) {
    int ret_status = RESULT_FAILURE;
    int SizeRequired = 0; // 结果字符数，size要大于此数才能装下结果
    // Path1SplitedCount 和 Path2SplitedCount : 以斜杠分割后的字符串数量；cnt_min：两者中的较小值
    int SplitedMinimum, Path1SplitedCount = 0, Path2SplitedCount = 0;
    // Path1 、 Path2 路径分割后的 char* 数组
    char **Path1SplitedResult = NULL, **Path2SplitedResult = NULL;
    int SameCountAfterSplited = 0; // 相同目录数量
    char *Path1Absoluted, *Path2Absoluted, *Path1Buffer, *Path2Buffer, *Token;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path1) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path1) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    Path1Buffer = malloc(PATH_MSIZE);
    Path2Buffer = malloc(PATH_MSIZE);
    Path1Absoluted = malloc(PATH_MSIZE);
    Path2Absoluted = malloc(PATH_MSIZE);
    if (NULL == Path1Absoluted || NULL == Path2Absoluted || NULL == Path1Buffer || NULL == Path2Buffer) {
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    if (!Path2)
        Path2 = PATH_CDIRS;
    if (OsPathAbsolutePath(Path1Absoluted, PATH_MSIZE, Path1))
        goto CleanAndReturn;
    if (OsPathAbsolutePath(Path2Absoluted, PATH_MSIZE, Path2))
        goto CleanAndReturn;
    if (OsPathSplitDrive(Path1Buffer, PATH_MSIZE, Path1Absoluted, PATH_MSIZE, Path1Absoluted))
        goto CleanAndReturn;
    if (OsPathSplitDrive(Path2Buffer, PATH_MSIZE, Path2Absoluted, PATH_MSIZE, Path2Absoluted))
        goto CleanAndReturn;
    if (strcmp(StrToLowercase(Path2Buffer), StrToLowercase(Path1Buffer))) {
        OsPathSetState(STATUS_OTHER_ERRORS);
        goto CleanAndReturn;
    }
    Path1SplitedResult = malloc(sizeof(char *) * strlen(Path1Absoluted));
    Path2SplitedResult = malloc(sizeof(char *) * strlen(Path2Absoluted));
    if (NULL == Path1SplitedResult || NULL == Path2SplitedResult) {
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    Token = strtok(Path1Absoluted, PATH_NSEPS);
    while (Token) {
        Path1SplitedResult[Path1SplitedCount++] = Token;
        Token = strtok(NULL, PATH_NSEPS);
    }
    Token = strtok(Path2Absoluted, PATH_NSEPS);
    while (Token) {
        Path2SplitedResult[Path2SplitedCount++] = Token;
        Token = strtok(NULL, PATH_NSEPS);
    }
    SplitedMinimum = Path1SplitedCount < Path2SplitedCount ? Path1SplitedCount : Path2SplitedCount;
    for (; SameCountAfterSplited < SplitedMinimum; ++SameCountAfterSplited) {
        strcpy(Path1Buffer, Path1SplitedResult[SameCountAfterSplited]);
        strcpy(Path2Buffer, Path2SplitedResult[SameCountAfterSplited]);
        if (strcmp(StrToLowercase(Path1Buffer), StrToLowercase(Path2Buffer)))
            break;
    }
    // 将 Path1Buffer 或 Path2Buffer 重置为空字符以复用其内存空间
    Path2Buffer[0] = EMPTY_CHAR;
    for (int i = 0; i < (Path2SplitedCount - SameCountAfterSplited); ++i) {
        SizeRequired += 3;
        if (SizeRequired >= PATH_MSIZE) {
            OsPathSetState(STATUS_PATH_TOO_LONG);
            goto CleanAndReturn;
        }
        strcat(Path2Buffer, PATH_PDIRS);
        strcat(Path2Buffer, PATH_NSEPS);
    }
    for (int i = SameCountAfterSplited; i < Path1SplitedCount; ++i) {
        SizeRequired += (int)strlen(Path1SplitedResult[i]);
        if (SizeRequired >= PATH_MSIZE) {
            OsPathSetState(STATUS_PATH_TOO_LONG);
            goto CleanAndReturn;
        }
        strcat(Path2Buffer, Path1SplitedResult[i]);
        if (i != Path1SplitedCount - 1) {
            ++SizeRequired;
            if (SizeRequired >= PATH_MSIZE) {
                OsPathSetState(STATUS_PATH_TOO_LONG);
                goto CleanAndReturn;
            }
            strcat(Path2Buffer, PATH_NSEPS);
        }
    }
    if (!strlen(Path2Buffer)) {
        if (BufferSize <= 1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            goto CleanAndReturn;
        }
        strcat(Buffer, PATH_CDIRS);
        ret_status = RESULT_SUCCESS;
        goto CleanAndReturn;
    }
    if (BufferSize <= strlen(Path2Buffer)) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        goto CleanAndReturn;
    }
    strcpy(Buffer, Path2Buffer);
    ret_status = RESULT_SUCCESS;
    goto CleanAndReturn;
CleanAndReturn:
    if (NULL != Path1Absoluted)
        free(Path1Absoluted);
    if (NULL != Path2Absoluted)
        free(Path2Absoluted);
    if (NULL != Path1Buffer)
        free(Path1Buffer);
    if (NULL != Path2Buffer)
        free(Path2Buffer);
    if (NULL != Path1SplitedResult)
        free(Path1SplitedResult);
    if (NULL != Path2SplitedResult)
        free(Path2SplitedResult);
    return ret_status;
}

// 生成给定路径的绝对路径
// 成功返回0，失败返回1
int OsPathAbsolutePath(char Buffer[], size_t BufferSize, const char *Path) {
    size_t Length;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    Length = GetFullPathNameA(Path, (DWORD)BufferSize, Buffer, NULL);
    if (Length <= 0 || Length > BufferSize) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        return RESULT_FAILURE;
    }
    return (OsPathNormpath(Buffer, BufferSize)) ? RESULT_SUCCESS : RESULT_FAILURE;
}

#else // _WIN32 else POSIX

// 验证路径是否是绝对路径
// 是：返回 1，否：返回 0
bool OsPathIsAbsolute(const char *Path) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
    return *Path == PATH_NSEP;
}

// 在posix上没有效果
// 返回path指针
char *OsPathNormcase(char Path[]) {
    OsPathSetState(STATUS_EXEC_SUCCESS);
    return Path;
}

// 规范化路径，消除双斜线等，例如 A//B、A/./B 和 A/foo/../B 都变成 A/B
// 应该理解，如果路径包含符号链接，这可能会改变路径的含义
// 改变path的时候，字符串长度可能会变，多数时候不变或变短，但空字符串会变为1字符
//    所以，如果path字符串长度为 0，也要保证字符串数组path空间大于等于2
// 此函数改变原路径，成功返回缓冲区指针，失败返回NULL
char *OsPathNormpath(char Path[], size_t Size) {
    char initial_slashes[3] = {0};
    char PathBufferTemp[PATH_MSIZE];
    char tmp_split[PATH_MSIZE];
    char *FinalResult = Path;
    char *SplitToken;
    char *PathSplitedList[PATH_MSIZE];
    size_t Index = 0, SizeRequired = 0;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return NULL;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return NULL;
    }
    if (!*Path) {
        if (Size < 2) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return NULL;
        }
        strcpy(Path, PATH_CDIRS);
        return FinalResult;
    }
    if (Path[0] == PATH_NSEP)
        initial_slashes[0] = PATH_NSEP;
    // Posix 允许一个或两个初始斜杠，但将三个或更多视为单斜杠
    if (Path[0] == PATH_NSEP && Path[1] == PATH_NSEP && !Path[2] == PATH_NSEP)
        initial_slashes[1] = PATH_NSEP;
    strcpy(tmp_split, Path);
    SplitToken = strtok(tmp_split, PATH_NSEPS);
    while (SplitToken) {
        if (strcmp(SplitToken, PATH_CDIRS) == 0)
            goto Next;
        if (strcmp(SplitToken, PATH_PDIRS) != 0 || (!*initial_slashes && Index == 0) || (Index > 0 && strcmp(PathSplitedList[Index - 1], PATH_PDIRS) == 0))
            PathSplitedList[Index++] = SplitToken;
        else if (Index > 0)
            --Index;
    Next:
        SplitToken = strtok(NULL, PATH_NSEPS);
    }
    if (Size <= strlen(initial_slashes)) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        return NULL;
    }
    SizeRequired += strlen(initial_slashes);
    strcpy(PathBufferTemp, initial_slashes);
    for (int i = 0; i < Index; ++i) {
        SizeRequired += strlen(PathSplitedList[i]);
        if (Size <= SizeRequired) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return NULL;
        }
        strcat(PathBufferTemp, PathSplitedList[i]);
        if (i != Index - 1)
            strcat(PathBufferTemp, PATH_NSEPS);
    }
    if (*PathBufferTemp) {
        strcpy(Path, PathBufferTemp);
    } else {
        if (Size > 1) {
            strcpy(Path, PATH_CDIRS);
        } else {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return NULL;
        }
    }
    return FinalResult;
}

// 将路径分割为驱动器号和路径
// 在posix平台上，驱动器号总是空字符串
// 成功返回0，失败返回1
int OsPathSplitDrive(char DriveBuffer[], size_t DriveBufSize, char PathBuffer[], size_t PathBufferSize, const char *Path) {
    char PathBufferTemp[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(PathBufferTemp, Path);
    if (NULL != DriveBuffer) {
        if (DriveBufSize < 1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        DriveBuffer[0] = EMPTY_CHAR;
    }
    if (NULL != PathBuffer) {
        if (PathBufferSize <= strlen(PathBufferTemp)) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(PathBuffer, PathBufferTemp);
    }
    return RESULT_SUCCESS;
}

// 用系统标准路径分隔符连接第4个及其后的所有参数值
// 注意，要被拼接的路径不应以斜杠开头，否则会覆盖前面除驱动器号以外的路径
// 参数n指示了它后面的参数个数
// 成功返回0，失败返回1
int OsPathJoinPath(char Buffer[], size_t BufferSize, int NumOfParam, const char *Path, ...) {
    size_t SizeRequired = 0;
    char *EachArgument, ResultPath[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    SizeRequired += strlen(Path);
    if (SizeRequired >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(ResultPath, Path);
    va_list ArgumentList;
    va_start(ArgumentList, Path);
    for (int i = 0; i < NumOfParam - 1; ++i) {
        EachArgument = va_arg(ArgumentList, char *);
        if (EachArgument[0] == PATH_NSEP) {
            SizeRequired = strlen(EachArgument);
            if (SizeRequired >= PATH_MSIZE) {
                OsPathSetState(STATUS_PATH_TOO_LONG);
                return RESULT_FAILURE;
            }
            strcpy(ResultPath, EachArgument);
        } else if (!*ResultPath || ResultPath[strlen(ResultPath) - 1] == PATH_NSEP) {
            SizeRequired += strlen(EachArgument);
            if (SizeRequired >= PATH_MSIZE) {
                OsPathSetState(STATUS_PATH_TOO_LONG);
                return RESULT_FAILURE;
            }
            strcat(ResultPath, EachArgument);
        } else {
            SizeRequired += 1 + strlen(EachArgument);
            if (SizeRequired >= PATH_MSIZE) {
                OsPathSetState(STATUS_PATH_TOO_LONG);
                return RESULT_FAILURE;
            }
            strcat(ResultPath, PATH_NSEPS);
            strcat(ResultPath, EachArgument);
        }
    }
    va_end(ArgumentList);
    if (BufferSize <= SizeRequired) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        return RESULT_FAILURE;
    }
    strcpy(Buffer, ResultPath);
    return RESULT_SUCCESS;
}

// 将路径按最后一个路径分隔符(斜杠)分割为两部分
// 成功返回0，失败返回1
int OsPathSplitPath(char HeadBuffer[], size_t HeadBufSize, char TailBuffer[], size_t TailBufSize, const char *Path) {
    size_t Length, IndexOfLastSepPlus1;
    char head[PATH_MSIZE];
    char PathBufferTemp[PATH_MSIZE];
    char stk_tmp[PATH_MSIZE];
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(PathBufferTemp, Path);
    Length = IndexOfLastSepPlus1 = strlen(PathBufferTemp);
    while (IndexOfLastSepPlus1 && PathBufferTemp[IndexOfLastSepPlus1 - 1] != PATH_NSEP)
        --IndexOfLastSepPlus1;
    if (IndexOfLastSepPlus1 > 0)
        strncpy(head, PathBufferTemp, IndexOfLastSepPlus1);
    head[IndexOfLastSepPlus1] = EMPTY_CHAR;
    strcpy(stk_tmp, head);
    if (*head && strtok(stk_tmp, PATH_NSEPS)) {
        for (int i = strlen(head) - 1; i >= 0; --i) {
            if (head[i] == PATH_NSEP)
                continue;
            else {
                head[i + 1] = EMPTY_CHAR;
                break;
            }
        }
    }
    if (NULL != HeadBuffer) {
        if (HeadBufSize <= strlen(head)) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(HeadBuffer, head);
    }
    if (NULL != TailBuffer) {
        if (TailBufSize <= Length - IndexOfLastSepPlus1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(TailBuffer, PathBufferTemp + IndexOfLastSepPlus1);
    }
    return RESULT_SUCCESS;
}

// 获取路径的上一级路径
// 成功返回字符指针，失败返回NULL
char *OsPathDirName(char Buffer[], size_t BufferSize, const char *Path) {
    static char DirPath[PATH_MSIZE];
    if (!Buffer) {
        BufferSize = PATH_MSIZE;
        Buffer = DirPath;
    }
    return OsPathSplitPath(Buffer, BufferSize, NULL, 0, Path) ? NULL : Buffer;
}

// 获取路径中的文件名
// 成功返回字符指针，失败返回NULL
char *OsPathBaseName(char Buffer[], size_t BufferSize, const char *Path) {
    static char BaseName[PATH_MSIZE];
    if (!Buffer) {
        BufferSize = PATH_MSIZE;
        Buffer = BaseName;
    }
    return OsPathSplitPath(NULL, 0, Buffer, BufferSize, Path) ? NULL : Buffer;
}

// 功能：生成相对路径
// 生成的相对路径是_path相对于start的路径
// 成功返回0，失败返回1
int OsPathRelativePath(char Buffer[], size_t BufferSize, const char *Path, const char *Path2) {
    int FinalReturnCode = RESULT_SUCCESS;
    int SizeRequired = 0; // 结果字符数，size要大于此数才能装下结果
    // cnt_p及cnt_s：以斜杠分割后的字符串数量；cnt_min：两者中的较小值
    int SplitedMinimum, Path1SplitedCount = 0, Path2SplitedCount = 0;
    char **Path1SplitedResult = NULL, **Path2SplitedResult = NULL;
    int SameCountAfterSplited = 0; // 相同目录数量
    char *Path1Absoluted, *Path2Absoluted, *Token, *PathBufferTemp;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    Path1Absoluted = malloc(PATH_MSIZE);
    Path2Absoluted = malloc(PATH_MSIZE);
    PathBufferTemp = malloc(PATH_MSIZE);
    if (!Path1Absoluted || !Path2Absoluted || !PathBufferTemp) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    if (!Path2)
        Path2 = PATH_CDIRS;
    if (OsPathAbsolutePath(Path1Absoluted, PATH_MSIZE, Path)) {
        FinalReturnCode = RESULT_FAILURE;
        goto CleanAndReturn;
    }
    if (OsPathAbsolutePath(Path2Absoluted, PATH_MSIZE, Path2)) {
        FinalReturnCode = RESULT_FAILURE;
        goto CleanAndReturn;
    }
    if (strlen(Path1Absoluted) <= 0)
        Path1SplitedResult = malloc(sizeof(char *));
    else
        Path1SplitedResult = malloc(sizeof(char *) * strlen(Path1Absoluted));
    if (strlen(Path2Absoluted) <= 0)
        Path2SplitedResult = malloc(sizeof(char *));
    else
        Path2SplitedResult = malloc(sizeof(char *) * strlen(Path2Absoluted));
    if (!Path1SplitedResult || !Path2SplitedResult) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto CleanAndReturn;
    }
    Token = strtok(Path1Absoluted, PATH_NSEPS);
    while (Token) {
        Path1SplitedResult[Path1SplitedCount++] = Token;
        Token = strtok(NULL, PATH_NSEPS);
    }
    Token = strtok(Path2Absoluted, PATH_NSEPS);
    while (Token) {
        Path2SplitedResult[Path2SplitedCount++] = Token;
        Token = strtok(NULL, PATH_NSEPS);
    }
    SplitedMinimum = Path1SplitedCount < Path2SplitedCount ? Path1SplitedCount : Path2SplitedCount;
    for (; SameCountAfterSplited < SplitedMinimum; ++SameCountAfterSplited) {
        if (strcmp(Path1SplitedResult[SameCountAfterSplited], Path2SplitedResult[SameCountAfterSplited]))
            break;
    }
    // 先将p_tmp重置为空字符串
    PathBufferTemp[0] = EMPTY_CHAR;
    for (int i = 0; i < (Path2SplitedCount - SameCountAfterSplited); ++i) {
        SizeRequired += 3;
        if (SizeRequired >= PATH_MSIZE) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_PATH_TOO_LONG);
            goto CleanAndReturn;
        }
        strcat(PathBufferTemp, PATH_PDIRS);
        strcat(PathBufferTemp, PATH_NSEPS);
    }
    for (int i = SameCountAfterSplited; i < Path1SplitedCount; ++i) {
        SizeRequired += strlen(Path1SplitedResult[i]);
        if (SizeRequired >= PATH_MSIZE) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_PATH_TOO_LONG);
            goto CleanAndReturn;
        }
        strcat(PathBufferTemp, Path1SplitedResult[i]);
        if (i != Path1SplitedCount - 1) {
            ++SizeRequired;
            if (SizeRequired >= PATH_MSIZE) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_PATH_TOO_LONG);
                goto CleanAndReturn;
            }
            strcat(PathBufferTemp, PATH_NSEPS);
        }
    }
    if (!strlen(PathBufferTemp)) {
        if (BufferSize <= 1) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_INSFC_BUFFER);
            goto CleanAndReturn;
        }
        strcat(Buffer, PATH_CDIRS);
        goto CleanAndReturn;
    }
    if (BufferSize <= strlen(PathBufferTemp)) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_INSFC_BUFFER);
        goto CleanAndReturn;
    }
    strcpy(Buffer, PathBufferTemp);
CleanAndReturn:
    if (NULL != Path1Absoluted)
        free(Path1Absoluted);
    if (NULL != Path2Absoluted)
        free(Path2Absoluted);
    if (NULL != PathBufferTemp)
        free(PathBufferTemp);
    if (NULL != Path1SplitedResult)
        free(Path1SplitedResult);
    if (NULL != Path2SplitedResult)
        free(Path2SplitedResult);
    return FinalReturnCode;
}

// 功能：生成给定路径的绝对路径
// 成功返回0，失败返回1
int OsPathAbsolutePath(char Buffer[], size_t BufferSize, const char *Path) {
    char PathBufferTemp[PATH_MSIZE] = {0};
    char *end_ch = PathBufferTemp;
    if (!OsPathIsAbsolute(Path)) {
        if (OsPathLastState())
            return RESULT_FAILURE;
        if (!OsPathGetCWD(PathBufferTemp, PATH_MSIZE))
            return RESULT_FAILURE;
    }
    if (strlen(PathBufferTemp) + strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    while (*end_ch)
        ++end_ch;
    if (end_ch != PathBufferTemp)
        --end_ch;
    if (*end_ch != PATH_NSEP && *Path != PATH_NSEP) {
        if (strlen(PathBufferTemp) + 1 >= PATH_MSIZE) {
            OsPathSetState(STATUS_PATH_TOO_LONG);
            return RESULT_FAILURE;
        }
        strcat(PathBufferTemp, PATH_NSEPS);
    }
    strcat(PathBufferTemp, Path);
    if (!OsPathNormpath(PathBufferTemp, PATH_MSIZE))
        return RESULT_FAILURE;
    if (BufferSize <= strlen(PathBufferTemp)) {
        OsPathSetState(STATUS_INSFC_BUFFER);
        return RESULT_FAILURE;
    }
    strcpy(Buffer, PathBufferTemp);
    return RESULT_SUCCESS;
}

#endif // _WIN32
////////////////////////////////////////////////////////////////////////////////

// 功能：将路径分割为[路径，扩展名]，扩展名包括'.'号
// 成功返回0，失败返回1
int OsPathSplitExt(char HeadBuffer[], size_t HeadBufSize, char ExtBuffer[], size_t ExtBufSize, const char *Path, int ExtSep) {
    size_t PathLength, DotIndex;
    char *pNormSepIndex, *pAltSepIndex, *pDotIndex;
    char PathBufferTemp[PATH_MSIZE];
    const char *pNameIndex;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    PathLength = strlen(Path);
    if (PathLength >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    strcpy(PathBufferTemp, Path);
    pNormSepIndex = strrchr(PathBufferTemp, PATH_NSEP);
#ifdef _WIN32
    pAltSepIndex = strrchr(PathBufferTemp, PATH_ASEP);
    if (pAltSepIndex > pNormSepIndex)
        pNormSepIndex = pAltSepIndex;
#endif
    if (!ExtSep)
        ExtSep = PATH_ESEP;
    pDotIndex = strrchr(PathBufferTemp, ExtSep);
    if (pDotIndex > pNormSepIndex) {
        if (NULL != pNormSepIndex)
            pNameIndex = pNormSepIndex + 1;
        else
            pNameIndex = PathBufferTemp;
        while (pNameIndex < pDotIndex) {
            if (*pNameIndex != ExtSep) {
                DotIndex = pDotIndex - PathBufferTemp;
                if (NULL != HeadBuffer) {
                    if (HeadBufSize <= DotIndex) {
                        OsPathSetState(STATUS_INSFC_BUFFER);
                        return RESULT_FAILURE;
                    }
                    strncpy(HeadBuffer, PathBufferTemp, DotIndex);
                    HeadBuffer[DotIndex] = EMPTY_CHAR;
                }
                if (NULL != ExtBuffer) {
                    if (ExtBufSize <= PathLength - DotIndex) {
                        OsPathSetState(STATUS_INSFC_BUFFER);
                        return RESULT_FAILURE;
                    }
                    strcpy(ExtBuffer, pDotIndex);
                }
                return RESULT_SUCCESS;
            }
            ++pNameIndex;
        }
    }
    if (NULL != HeadBuffer) {
        if (HeadBufSize <= PathLength) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        strcpy(HeadBuffer, PathBufferTemp);
    }
    if (NULL != ExtBuffer) {
        if (ExtBufSize < 1) {
            OsPathSetState(STATUS_INSFC_BUFFER);
            return RESULT_FAILURE;
        }
        ExtBuffer[0] = EMPTY_CHAR;
    }
    return RESULT_SUCCESS;
}

// 功能：重定位路径中的'.'和'..'并把不能重定位的'..'修剪掉
// 成功返回0，失败返回1
int OsPathPrunePath(char Buffer[], size_t BufferSize, const char *Path) {
    size_t NewPathTotalSize = 0;
    int TokenCount = 0;
    int FinalReturnCode = RESULT_SUCCESS;
    char **PathSplitedList;
    char *Token, *PathBufferTemp1, *PathBufferTemp2;
    OsPathSetState(STATUS_EXEC_SUCCESS);
    if (!Path) {
        OsPathSetState(STATUS_EMPTY_POINTER);
        return RESULT_FAILURE;
    }
    if (strlen(Path) >= PATH_MSIZE) {
        OsPathSetState(STATUS_PATH_TOO_LONG);
        return RESULT_FAILURE;
    }
    PathBufferTemp1 = malloc(PATH_MSIZE * sizeof(char));
    PathBufferTemp2 = malloc(PATH_MSIZE * sizeof(char));
    PathSplitedList = malloc(PATH_MSIZE * sizeof(char *));
    if (!PathBufferTemp1 || !PathBufferTemp2 || !PathSplitedList) {
        FinalReturnCode = RESULT_FAILURE;
        OsPathSetState(STATUS_MEMORY_ERROR);
        goto ClearAndReturn;
    }
    strcpy(PathBufferTemp1, Path);
    if (!OsPathNormpath(PathBufferTemp1, PATH_MSIZE)) {
        FinalReturnCode = RESULT_FAILURE;
        goto ClearAndReturn;
    }
    Token = strtok(PathBufferTemp1, PATH_NSEPS);
    while (Token) {
        if (TokenCount > PATH_MSIZE) {
            FinalReturnCode = RESULT_FAILURE;
            OsPathSetState(STATUS_PATH_TOO_LONG);
            goto ClearAndReturn;
        }
        if (strcmp(Token, PATH_PDIRS)) {
            PathSplitedList[TokenCount++] = Token;
        }
        Token = strtok(NULL, PATH_NSEPS);
    }
    if (NULL != Buffer) {
        PathBufferTemp2[0] = EMPTY_CHAR;
        for (int i = 0; i < TokenCount; ++i) {
            NewPathTotalSize += strlen(PathSplitedList[i]) + 1;
            if (NewPathTotalSize >= BufferSize) {
                FinalReturnCode = RESULT_FAILURE;
                OsPathSetState(STATUS_INSFC_BUFFER);
                goto ClearAndReturn;
            }
            strcat(PathBufferTemp2, PathSplitedList[i]);
            if (i != TokenCount - 1)
                strcat(PathBufferTemp2, PATH_NSEPS);
        }
        strcpy(Buffer, PathBufferTemp2);
    }
ClearAndReturn:
    if (NULL != PathBufferTemp2)
        free(PathBufferTemp2);
    if (NULL != PathSplitedList)
        free(PathSplitedList);
    if (NULL != PathBufferTemp1)
        free(PathBufferTemp1);
    return FinalReturnCode;
}
