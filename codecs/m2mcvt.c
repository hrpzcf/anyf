#ifdef _WIN32
#include <windows.h>

// 将本地ANSI代码页字符串转换为UTF8字符串
// 参数buf：接收转换后字符串的缓冲区
// 参数bfsize：缓冲区大小
// 参数ansiSTR：当前代码页编码的字符串
// 返回值：函数返回写入缓冲区buf的字节数
// 如果参数buf为NULL或bfsize为0，则返回容纳新字符串所需的缓冲区大小，以字符为单位
// 注意：待转换字符一定要保证以'\\0'结束，否则一直寻找到'\\0'，即使数组越界也是如此
int StringANSIToUTF8(char buf[], int bfsize, char *ansiSTR) {
    int length;
    char *ansi_tmp;
    if (NULL == (ansi_tmp = malloc(strlen(ansiSTR) + 1)))
        return 0;
    strcpy(ansi_tmp, ansiSTR);
    length = MultiByteToWideChar(CP_ACP, 0, ansi_tmp, -1, NULL, 0);
    if (length <= 0)
        return 0;
    WCHAR *new_wchars = malloc(sizeof(WCHAR) * length);
    if (NULL == new_wchars)
        return 0;
    length = MultiByteToWideChar(CP_ACP, 0, ansi_tmp, -1, new_wchars, length);
    if (length <= 0)
        return 0;
    if (NULL == buf || bfsize <= 0)
        length = WideCharToMultiByte(CP_UTF8, 0, new_wchars, -1, NULL, 0, NULL, NULL);
    else
        length = WideCharToMultiByte(CP_UTF8, 0, new_wchars, -1, buf, bfsize, NULL, NULL);
    if (NULL != ansi_tmp)
        free(ansi_tmp);
    if (NULL != new_wchars)
        free(new_wchars);
    return length;
}

// 将UTF8字符串转换为本地ANSI代码页字符串
// 参数buf：接收转换后字符串的缓冲区
// 参数bfsize：缓冲区大小
// 参数utf8STR：UTF-8编码的字符串
// 返回值：函数返回写入缓冲区buf的字节数
// 如果参数buf为NULL或bfsize为0，则返回容纳新字符串所需的缓冲区大小，以字符为单位
// 注意：待转换字符一定要保证以'\\0'结束，否则一直寻找到'\\0'，即使数组越界也是如此
int StringUTF8ToANSI(char buf[], int bfsize, char *utf8STR) {
    int length;
    char *utf8_tmp;
    if (NULL == (utf8_tmp = malloc(strlen(utf8STR) + 1)))
        return 0;
    strcpy(utf8_tmp, utf8STR);
    length = MultiByteToWideChar(CP_UTF8, 0, utf8_tmp, -1, NULL, 0);
    if (length <= 0)
        return 0;
    WCHAR *new_wchars = malloc(sizeof(WCHAR) * length);
    if (NULL == new_wchars)
        return 0;
    length = MultiByteToWideChar(CP_UTF8, 0, utf8_tmp, -1, new_wchars, length);
    if (length <= 0)
        return 0;
    if (NULL == buf || bfsize <= 0)
        length = WideCharToMultiByte(CP_ACP, 0, new_wchars, -1, NULL, 0, NULL, NULL);
    else
        length = WideCharToMultiByte(CP_ACP, 0, new_wchars, -1, buf, bfsize, NULL, NULL);
    if (NULL != utf8_tmp)
        free(utf8_tmp);
    if (NULL != new_wchars)
        free(new_wchars);
    return length;
}

#endif // _WIN32
