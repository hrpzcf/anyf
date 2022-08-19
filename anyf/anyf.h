#ifndef __ANYF_H
#define __ANYF_H
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../ospath/ospath.h"

#define ANYF_VER "0.1.3"

#define FILE_MAX      LLONG_MAX  // 文件最大字节数
#define PM            PATH_MSIZE // 路径最大字节数
#define MESSAGE_INFO  "\33[32m[信息]\33[0m "
#define MESSAGE_WARN  "\33[33m[警告]\33[0m "
#define MESSAGE_ERROR "\33[31m[错误]\33[0m "

#ifdef _WIN32
#define AnyfSeek _fseeki64
#define AnyfTell _ftelli64
#define inline _inline
int StringANSIToUTF8(char buf[], int bfsize, char *ansiSTR);
int StringUTF8ToANSI(char buf[], int bfsize, char *utf8STR);
#else // else posix
#define AnyfSeek fseek
#define AnyfTell ftell
#endif // _WIN32

// 不同平台上printf中的int64_t格式化占位符
#ifndef _WIN32
#ifdef __x86_64
// 64位linux平台的int64类型是(int long)
#define I64_SPECIFIER "ld"
#else
// 32位linux平台的int64类型是(int long long)
#define I64_SPECIFIER "lld"
#endif // __x86_64
#else
// 32和64位windows平台上int64_t都是(int long long)
#define I64_SPECIFIER "lld"
#endif // _WIN32

#define EXIT_CODE_SUCCESS 0 // 成功退出状态码
#define EXIT_CODE_FAILURE 1 // 失败退出状态码

#define ID_N 16  // head的id字段数组大小
#define SP_N 4   // head的sp字段数组大小
#define EM_N 256 // head的emt字段数组大小

// 文件读写缓冲区
typedef struct {
    int64_t size;
    char fdata[];
} BUFFER_T;

#define L_BUF_SIZE 8388608LL   // 文件读写缓冲区大小下限
#define U_BUF_SIZE 134217728LL // 文件读写缓冲区大小上限
#define DIR_SIZE   -1          // 定义目录本身大小为-1
#define EQS_MAX    512         // 显示子文件信息表时分隔符缓冲区大小

#define JPEG_SIG     0xFF // JPEG字段标记码
#define JPEG_START   0xD8 // JPEG图像起始
#define JPEG_END     0xD9 // JPEG图像结束
#define JPEG_INVALID 0    // 无效的JPEG图像
#define JPEG_ERROR   -1   // 检查JPEG图像过程中出现了错误

// 文件头信息集合
// 注意结构体成员的内存对齐
// 因为要把结构体直接写入到文件或从文件直接读取
#pragma pack(16)
typedef struct {
    char id[ID_N];    // 文件标识符
    char emt[EM_N];   // 预留空字节
    int16_t sp[SP_N]; // 文件规范版本
    int64_t count;    // 包含文件总数
} HEAD_T;             // 文件头信息结构体
#pragma pack()

// 子文件信息，包括文件大小,文件名长度,文件名
// 注意结构体成员的内存对齐
// 因为要把结构体直接写入到文件或从文件直接读取
#pragma pack(2)
typedef struct {
    int64_t offset; // 数据偏移量
    int64_t fsize;  // 数据块大小
    int16_t fnlen;  // 文件名长度
    char fname[PM]; // 文件名字符串
} INFO_T;
#pragma pack()

// 文件基本信息结构体
typedef struct {
    HEAD_T head;   // 文件的头信息
    int64_t start; // 标识符起始位置
    INFO_T *sheet; // 子文件信息表
    int64_t cells; // sheet 的容量
    char *path;    // 文件的绝对路径
    FILE *handle;  // 打开的二进制流
} ANYF_T;

// 默认[anyf]文件头信息结构体
static const HEAD_T DEFAULT_HEAD = {
    // 分别为：2位年份，主版本，次版本，修订版本
    .sp = {22, 1, 0, 6},
    // 预留的空字节用于可能增加的信息字段
    .emt = {0},
    // [anyf]文件中包含的子文件总数，初始总数总是设置为零
    .count = 0LL,
    // 格式标识："\377Anyf Momo\0"等16字节，余下字节为零
    .id = {0xFF, 0x41, 0x6e, 0x79, 0x66, 0x20, 0x4d, 0x6f, 0x4d, 0x6f},
};

// 出错时打印调试信息及退出程序
#define PRINT_ERROR_AND_ABORT(STR) \
    fprintf(stderr, MESSAGE_ERROR STR ": 源码 %s 第 %d 行，版本 %s\n", OsPathBaseName(NULL, 0ULL, __FILE__), __LINE__, ANYF_VER); \
    exit(EXIT_CODE_FAILURE)

// 出错时判断是否关闭文件流并删除文件
#define WHETHER_CLOSE_REMOVE(ANYFTYPE) \
    if (ANYFTYPE->head.count <= 0) { \
        fclose(ANYFTYPE->handle), remove(ANYFTYPE->path); \
    }

#define FS_S (sizeof(int64_t))        // bom的fsize字段大小
#define NL_S (sizeof(int16_t))        // bom的fnlen字段大小
#define ID_S (ID_N * sizeof(char))    // head的id字段大小
#define SP_S (SP_N * sizeof(int16_t)) // head的sp字段大小
#define EM_S (EM_N * sizeof(char))    // head的emt字段大小
#define FC_S (sizeof(int64_t))        // head的count字段大小

#define FCNT_O (ID_S + SP_S + EM_S)        // fcount字段在[anyf]文件中的偏移量
#define DATA_O (ID_S + SP_S + EM_S + FC_S) // 数据块起始偏移量
#define FSNL_S (FS_S + NL_S)               // 结构体finfo_t中fsize和fnlen的类型大小之和

ANYF_T *AnyfMake(const char *AnyfPath, bool Overwrite);
ANYF_T *AnyfOpen(const char *AnyfPath);
void AnyfClose(ANYF_T *AnyfType);
ANYF_T *AnyfPack(const char *ToPack, bool Recursion, ANYF_T *AnyfType, bool Append);
ANYF_T *AnyfExtract(const char *ToExtract, const char *Destination, int Overwrite, ANYF_T *AnyfType);
ANYF_T *AnyfInfo(const char *AnyfPath);
bool AnyfIsFakeJPEG(const char *FakeJPEGPath);
ANYF_T *AnyfMakeFakeJPEG(const char *AnyfPath, const char *JPEGPath, bool Overwrite);
ANYF_T *AnyfOpenFakeJPEG(const char *FakeJPEGPath);

#endif //__ANYF_H
