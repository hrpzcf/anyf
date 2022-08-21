#ifndef __ANYF_H
#define __ANYF_H
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../ospath/ospath.h"

#define ANYF_VER "0.1.7"

#define FILE_MAX      LLONG_MAX     // 文件最大字节数
#define PMS           PATH_MAX_SIZE // 路径最大字节数
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

#define ID_COUNT  16  // HEAD_T 的 id 数组元素个数
#define STD_COUNT 4   // HEAD_T 的 std 数组元素个数
#define EMT_COUNT 256 // HEAD_T 的 emt 数组元素个数

// 文件读写缓冲区
typedef struct {
    int64_t size;
    char fdata[];
} BUFFER_T;

#define BUF_SIZE_L 8388608LL   // 文件读写缓冲区大小下限
#define BUF_SIZE_U 134217728LL // 文件读写缓冲区大小上限
#define DIR_SIZE   -1          // 定义：目录本身大小为 -1
#define EQUAL_MAX  512         // 显示子文件信息时分隔符(等号)缓冲区大小

#define JPEG_SIG   0xFF // 此字节表示其后一个字节是 JPEG 标记码
#define JPEG_START 0xD8 // 跟在 JPEG_SIG 后，表示 JPEG 图像起始
#define JPEG_END   0xD9 // 跟在 JPEG_SIG 后，表示 JPEG 图像结束

#define JPEG_INVALID 0  // 无效的 JPEG 图像
#define JPEG_ERROR   -1 // 检查 JPEG 图像过程中出现了错误

// 文件头信息集合
// 注意结构体成员的内存对齐
// 因为要把结构体直接写入到文件或从文件直接读取
#pragma pack(16)
typedef struct {
    char id[ID_COUNT];      // 文件标识符
    char emt[EMT_COUNT];    // 预留空字节
    int16_t std[STD_COUNT]; // 文件规范版本
    int64_t count;          // 包含文件总数
} HEAD_T;                   // 文件头信息结构体
#pragma pack()

// 子文件信息，包括文件大小,文件名长度,文件名
// 注意结构体成员的内存对齐
// 因为要把结构体直接写入到文件或从文件直接读取
// 写入[ANYF]文件时从 fsize 开始写，offset 不写入文件
// 子文件信息：<fsize、fnlen、fname、子文件字节码>为一个子文件信息
#pragma pack(2)
typedef struct {
    int64_t offset;  // 子文件信息在[ANYF]中的偏移量
    int64_t fsize;   // 子文件数据内容的字节数大小
    int16_t fnlen;   // 子文件的文件名长度
    char fname[PMS]; // 子文件的文件名字符串，UTF8编码
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

// 默认[ANYF]文件头信息，可修改 id 内容以自定义文件标识
static const HEAD_T DEFAULT_HEAD = {
    // 格式标识："\377Anyf Momo\0"等16字节，余下为零
    .id =
        {
            0xff,
            0x41, // 'A'
            0x6e, // 'n'
            0x79, // 'y'
            0x66, // 'f'
            0x20, // ' '
            0x4d, // 'M'
            0x6f, // 'o'
            0x4d, // 'M'
            0x6f, // 'o'
        },
    // 分别为：2位年份，主版本，次版本，修订版本
    .std = {22, 1, 0, 6},
    // 预留的 256 个字节用于可能增加的信息
    .emt = {0},
    // [ANYF]文件中包含的子文件总数，初始总数总是设置为零
    .count = 0LL,
};

// 出错时打印调试信息并退出程序
#define PRINT_ERROR_AND_ABORT(STR) \
    fprintf(stderr, MESSAGE_ERROR STR ": 源码 %s 第 %d 行，版本 %s\n", OsPathBaseName(NULL, 0ULL, __FILE__), __LINE__, ANYF_VER); \
    exit(EXIT_CODE_FAILURE)

// 出错时判断是否关闭文件流并删除文件
#define WHETHER_CLOSE_REMOVE(ANYFTYPE) \
    if (ANYFTYPE->head.count <= 0) { \
        fclose(ANYFTYPE->handle), remove(ANYFTYPE->path); \
    }

#define FSIZE_SIZE (sizeof(int64_t))             // INFO_T 的 fsize 成员大小
#define FNLEN_SIZE (sizeof(int16_t))             // INFO_T 的 fnlen 成员大小
#define ID_SIZE    (ID_COUNT * sizeof(char))     // HEAD_T 的 id 成员大小
#define STD_SIZE   (STD_COUNT * sizeof(int16_t)) // HEAD_T 的 std 成员大小
#define EMT_SIZE   (EMT_COUNT * sizeof(char))    // HEAD_T 的 emt 成员大小
#define COUNT_SIZE (sizeof(int64_t))             // HEAD_T 的 count 成员大小

#define COUNT_OFFSET     (ID_SIZE + STD_SIZE + EMT_SIZE)              // HEAD_T 中的 count 在[ANYF]文件中的偏移量
#define SUBDATA_OFFSET   (ID_SIZE + STD_SIZE + EMT_SIZE + COUNT_SIZE) // [ANYF]文件中首个子文件信息(见前面注释)起始偏移量
#define FSIZE_FNLEN_SIZE (FSIZE_SIZE + FNLEN_SIZE)                    // INFO_T 中 fsize 和 fnlen 两个成员的大小之和

ANYF_T *AnyfMake(const char *AnyfPath, bool Overwrite);
ANYF_T *AnyfOpen(const char *AnyfPath);
void AnyfClose(ANYF_T *AnyfType);
ANYF_T *AnyfPack(const char *ToBePacked, bool Recursion, ANYF_T *AnyfType, bool Append);
ANYF_T *AnyfExtract(const char *ToExtract, const char *Destination, int Overwrite, ANYF_T *AnyfType);
ANYF_T *AnyfInfo(const char *AnyfPath);
bool AnyfIsFakeJPEG(const char *FakeJPEGPath);
ANYF_T *AnyfMakeFakeJPEG(const char *AnyfPath, const char *JPEGPath, bool Overwrite);
ANYF_T *AnyfOpenFakeJPEG(const char *FakeJPEGPath);

#endif //__ANYF_H
