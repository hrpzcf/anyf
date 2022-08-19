#ifndef __INFO_H
#define __INFO_H

#define M2S1(x) #x
#define M2S2(x) M2S1(x)

#ifdef _MSC_VER
#define COMPILER "MSC " M2S2(_MSC_VER)
#elif defined(__GNUC__)
#define COMPILER "GCC "__VERSION__
#else
#define COMPILER "未知"
#endif

#define AUTHOR_INFO "作者：hrpzcf，源码：https://github.com/hrpzcf/anyf"

#ifdef _WIN32
#ifndef _WIN64
#define PLATFORM "win32"
#else
#define PLATFORM "win_x64"
#endif
#elif defined(__linux__)
#ifndef __x86_64
#define PLATFORM "linux"
#else
#define PLATFORM "linux_x64"
#endif
#else
#define PLATFORM "posix"
#endif

#define BUILT_INFO "名称：%s，版本：" ANYF_VER ", 平台: " PLATFORM ", 编译器: " COMPILER "，编译时间：" __DATE__ ", " __TIME__

#endif // __INFO_H
