#ifndef __OSPATH_H
#define __OSPATH_H

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS // 关闭MSC强制安全警告
#define _CRT_SECURE_NO_WARNINGS
#endif // _CRT_SECURE_NO_WARNINGS
#endif // _MSC_VER

#include <stdbool.h>
#include <stddef.h>

// 函数最后一次执行结束状态码
#define STATUS_EXEC_SUCCESS  0x00000000 // 函数执行成功
#define STATUS_COMMAND_FAIL  0x00000001 // 命令执行失败
#define STATUS_OTHER_ERRORS  0X00000002 // 其他出错原因
#define STATUS_PATH_TOO_LONG 0x00000004 // 路径太长
#define STATUS_MEMORY_ERROR  0x00000008 // 内存分配失败
#define STATUS_GET_ATTR_FAIL 0x00000010 // 获取属性出错
#define STATUS_EMPTY_POINTER 0x00000020 // 遇到空指针
#define STATUS_INVALID_PARAM 0x00000040 // 无效的参数值
#define STATUS_INSFC_BUFFER  0x00000080 // 缓冲区空间不足

// 函数 OsPathScanPath 的 Target 参数可选值
#define OSPATH_FILE 0x00000001 // 搜索文件
#define OSPATH_DIR  0x00000002 // 搜索目录
#define OSPATH_BOTH 0x00000004 // 文件与目录

#define RESULT_SUCCESS 0 // 成功
#define RESULT_FAILURE 1 // 失败

#define RESULT_TRUE  true  // 为真
#define RESULT_FALSE false // 为假

#define PATH_CDIRS "."  // 代表当前目录的字符串
#define PATH_PDIRS ".." // 代表上级目录的字符串
#define PATH_ESEP  '.'  // 代表扩展名分隔符的字符
#define PATH_ESEPS "."  // 代表扩展名分隔符的字符串

#define EMPTY_CHAR 0    // 空字符
#define PATH_MSIZE 4096 // 允许处理的最大路径字节数

#ifdef _WIN32
#define PATH_NSEP   '\\'   // 代表普通路径分隔符的字符
#define PATH_NSEPS  "\\"   // 代表普通路径分隔符的字符串
#define PATH_NSEPS2 "\\\\" // 代表双普通路径分隔符的字符串
#define PATH_ASEP   '/'    // 代表变体路径分隔符的字符
#define PATH_ASEPS  "/"    // 代表变体路径分隔符的字符串
#define PATH_UNCDS  "\\\\.\\"
#define PATH_UNCQS  "\\\\?\\"
#define PATH_COLON  ':' // 代表驱动器号与路径分隔符的字符
#define PATH_COLONS ":" // 代表驱动器号与路径分隔符的字符串
#else
#define PATH_NSEP  '/'  // 代表普通路径分隔符的字符
#define PATH_NSEPS "/"  // 代表普通路径分隔符的字符串
// 以下在POSIX平台上无效，仅用于检查
#define PATH_ASEP  '\\' // 代表变体路径分隔符的字符
#define PATH_ASEPS "\\" // 代表变体路径分隔符的字符串
#endif

typedef struct {
    size_t blocks; // 数组paths能容纳的指针数
    size_t count;  // 数组paths中已写入的字符指针数量
    char *paths[]; // 保存路径字符指针的指针数组
} SCANNER_T;

int OsPathLastState(void); //获取最后一次函数执行的错误状态
SCANNER_T *OsPathMakeScanner(size_t Blocks);
int OsPathDeleteScanner(SCANNER_T *Scanner);
int OsPathScanPath(const char *DirPath, int Target, int Recursion, SCANNER_T **const ppScanner);
bool OsPathExists(const char *Path);
bool OsPathIsDirectory(const char *Path);
bool OsPathIsFile(const char *Path);
bool OsPathIsAbsolute(const char *Path);
int OsPathMakeDIR(const char *Path);
char *OsPathGetCWD(char *Buffer, size_t Size);
char *OsPathNormcase(char *Path);
char *OsPathNormpath(char Path[], size_t Size);
int OsPathSplitDrive(char DriveBuffer[], size_t DriveBufSize, char PathBuffer[], size_t PathBufferSize, const char *Path);
int OsPathJoinPath(char Buffer[], size_t BufferSize, int NumOfParam, const char *Path, ...);
int OsPathSplitPath(char HeadBuffer[], size_t HeadBufSize, char TailBuffer[], size_t TailBufSize, const char *Path);
char *OsPathDirName(char Buffer[], size_t BufferSize, const char *Path);
char *OsPathBaseName(char Buffer[], size_t BufferSize, const char *Path);
int OsPathRelativePath(char Buffer[], size_t BufferSize, const char *Path1, const char *Path2);
int OsPathAbsolutePath(char Buffer[], size_t BufferSize, const char *Path);
int OsPathSplitExt(char HeadBuffer[], size_t HeadBufSize, char ExtBuffer[], size_t ExtBufSize, const char *Path, int ExtSep);
int OsPathPrunePath(char Buffer[], size_t BufferSize, const char *Path);

#endif // __OSPATH_H
