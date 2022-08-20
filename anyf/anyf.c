#include "anyf.h"

#include <stdlib.h>

// 扩充子文件信息表容量
static bool ExpandBOM(ANYF_T *AnyfType, size_t Capacity) {
    INFO_T *finfo_tmp;
    size_t exp_size;
    if (!AnyfType)
        return false;
    if (Capacity > INT64_MAX)
        return false;
    if (Capacity == 0ULL)
        Capacity = 1ULL;
    if (!AnyfType->sheet) {
        finfo_tmp = malloc(Capacity * sizeof(INFO_T));
        if (finfo_tmp) {
            AnyfType->cells = (int64_t)Capacity;
            AnyfType->sheet = finfo_tmp;
            return true;
        } else
            return false;
    }
    if (AnyfType->cells - AnyfType->head.count >= (int64_t)Capacity) {
        return true;
    }
    exp_size = (Capacity + AnyfType->head.count) * sizeof(INFO_T);
    finfo_tmp = realloc(AnyfType->sheet, exp_size);
    if (!finfo_tmp)
        return false;
    AnyfType->sheet = finfo_tmp;
    AnyfType->cells = (int64_t)Capacity + AnyfType->head.count;
    return true;
}

// 扩充文件读写缓冲区内存空间
static bool ExpandBUF(BUFFER_T **ppBuffer, int64_t Size) {
    BUFFER_T *BufferTemp;
    if (!ppBuffer || !*ppBuffer)
        return false;
    if ((*ppBuffer)->size >= Size)
        return true;
    BufferTemp = realloc(*ppBuffer, sizeof(BUFFER_T) + Size);
    if (!BufferTemp)
        return false;
    BufferTemp->size = Size, *ppBuffer = BufferTemp;
    return true;
}

// 大文件复制，从子文件流复制到[anyf]文件流
static bool SubCopyToMain(FILE *SubStream, FILE *AnyfFileStream, BUFFER_T **BufferRW) {
    size_t SizeRead; // 每次fread的大小
    if (!ExpandBUF(BufferRW, BUF_SIZE_U))
        return false;
    while (!feof(SubStream)) {
        SizeRead = fread((*BufferRW)->fdata, 1, BUF_SIZE_U, SubStream);
        if (SizeRead == 0ULL) {
            return true;
        } else if (SizeRead < 0ULL)
            return false;
        if (fwrite((*BufferRW)->fdata, SizeRead, 1, AnyfFileStream) != 1)
            return false;
    }
    return true;
}

// 大文件复制，从[anyf]文件流复制到提取子文件时新建的子文件流
static bool MainCopyToSub(FILE *AnyfFileStream, int64_t Offset, int64_t SizeToRead, FILE *SubStream, BUFFER_T **BufferRW) {
    if (!ExpandBUF(BufferRW, BUF_SIZE_U))
        return false;
    if (AnyfSeek(AnyfFileStream, Offset, SEEK_SET))
        return false;
    while (SizeToRead >= BUF_SIZE_U) {
        if (fread((*BufferRW)->fdata, BUF_SIZE_U, 1, AnyfFileStream) != 1)
            return false;
        if (fwrite((*BufferRW)->fdata, BUF_SIZE_U, 1, SubStream) != 1)
            return false;
        if ((SizeToRead -= BUF_SIZE_U) < BUF_SIZE_U) {
            if (SizeToRead > 0LL) {
                if (fread((*BufferRW)->fdata, SizeToRead, 1, AnyfFileStream) != 1)
                    return false;
                if (fwrite((*BufferRW)->fdata, SizeToRead, 1, SubStream) != 1)
                    return false;
            }
            return true;
        }
    }
    return true;
}

// 获取JPEG文件的净大小
static int64_t RealSizeOfJPEG(FILE *JPEGPath, int64_t TotalSize, BUFFER_T **BufferS8) {
    uint8_t *BufferU8;
    int64_t EndPos = 0;
    if (TotalSize < 4)
        return JPEG_INVALID;
    if ((*BufferS8)->size < TotalSize)
        if (!ExpandBUF(BufferS8, TotalSize))
            return JPEG_ERROR;
    rewind(JPEGPath);
    if (fread((*BufferS8)->fdata, 1, TotalSize, JPEGPath) != TotalSize) {
        return JPEG_ERROR;
    }
    BufferU8 = (uint8_t *)(*BufferS8)->fdata;
    if (!(BufferU8[0] == JPEG_SIG && BufferU8[1] == JPEG_START))
        return JPEG_INVALID;
    for (EndPos = 4; EndPos <= TotalSize; ++EndPos)
        if (BufferU8[EndPos - 2] == JPEG_SIG && BufferU8[EndPos - 1] == JPEG_END)
            return EndPos;
    return JPEG_INVALID;
}

// 判断是否是伪装的 JPEG 文件
bool AnyfIsFakeJPEG(const char *FakeJPEGPath) {
    FILE *FakeJPEGHandle;
    BUFFER_T *BufferRW;
    int64_t FakeJPEGSize, JPEGNetSize;
    HEAD_T HeadTemp;
    static char PathBuffer[PATH_MAX_SIZE];
    bool FinalReturnCode = false;
    if (OsPathAbsolutePath(PathBuffer, PATH_MAX_SIZE, FakeJPEGPath))
        return FinalReturnCode;
    FakeJPEGHandle = fopen(PathBuffer, "rb");
    if (!FakeJPEGHandle)
        return FinalReturnCode;
    if (AnyfSeek(FakeJPEGHandle, 0LL, SEEK_END))
        return FinalReturnCode;
    if ((FakeJPEGSize = AnyfTell(FakeJPEGHandle)) < 0LL)
        return FinalReturnCode;
    if (BufferRW = malloc(FakeJPEGSize + sizeof(HEAD_T) + sizeof(BUFFER_T))) {
        BufferRW->size = FakeJPEGSize + sizeof(HEAD_T);
    } else {
        return false;
    }
    JPEGNetSize = RealSizeOfJPEG(FakeJPEGHandle, FakeJPEGSize, &BufferRW);
    if (JPEGNetSize == JPEG_INVALID || JPEGNetSize == JPEG_ERROR)
        goto FreeAndReturn;
    if (FakeJPEGSize - JPEGNetSize < sizeof(HEAD_T)) {
        goto FreeAndReturn;
    }
    if (AnyfSeek(FakeJPEGHandle, JPEGNetSize, SEEK_SET))
        goto FreeAndReturn;
    if (fread(&HeadTemp, sizeof(HEAD_T), 1, FakeJPEGHandle) != 1)
        goto FreeAndReturn;
    if (memcmp(DEFAULT_HEAD.id, HeadTemp.id, sizeof(DEFAULT_HEAD.id)))
        goto FreeAndReturn;
    FinalReturnCode = true;
FreeAndReturn:
    free(BufferRW);
    fclose(FakeJPEGHandle);
    return FinalReturnCode;
}

// 关闭ANYF_T对象
void AnyfClose(ANYF_T *AnyfType) {
    if (AnyfType) {
        if (AnyfType->path)
            free(AnyfType->path);
        if (AnyfType->sheet)
            free(AnyfType->sheet);
        if (AnyfType->handle)
            fclose(AnyfType->handle);
        free(AnyfType);
    }
}

// 创建新[anyf]文件
ANYF_T *AnyfMake(const char *AnyfPath, bool Overwrite) {
    ANYF_T *AnyfType;               // [anyf]文件信息结构体
    FILE *AnyfHandle;               // [anyf]二进制文件流指针
    char PathBuffer[PATH_MAX_SIZE]; // 绝对路径及父目录缓冲
    char *AnyfPathCopied;           // 拷贝路径用于结构体
    if (OsPathAbsolutePath(PathBuffer, PATH_MAX_SIZE, AnyfPath)) {
        printf(MESSAGE_ERROR "无法获取[anyf]文件绝对路径：%s\n", AnyfPath);
        exit(EXIT_CODE_FAILURE);
    }
    printf(MESSAGE_INFO "创建[anyf]文件：%s\n", PathBuffer);
    AnyfPathCopied = malloc(strlen(PathBuffer) + 1ULL);
    if (!AnyfPathCopied) {
        PRINT_ERROR_AND_ABORT("为[anyf]文件路径数组分配内存失败");
    }
    strcpy(AnyfPathCopied, PathBuffer);
    if (OsPathExists(AnyfPathCopied)) {
        if (OsPathIsDirectory(AnyfPathCopied)) {
            printf(MESSAGE_ERROR "此位置已存在同名目录\n");
            exit(EXIT_CODE_FAILURE);
        } else if (OsPathLastState()) {
            printf(MESSAGE_ERROR "获取路径属性失败\n");
            exit(EXIT_CODE_FAILURE);
        } else if (!Overwrite) {
            printf(MESSAGE_ERROR "已存在同名文件但未指定覆盖\n");
            exit(EXIT_CODE_FAILURE);
        }
    } else if (OsPathLastState()) {
        printf(MESSAGE_ERROR "无法检查此路径是否存在\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathDirName(PathBuffer, PATH_MAX_SIZE, PathBuffer)) {
        printf(MESSAGE_ERROR "获取[anyf]文件的父目录路径失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathExists(PathBuffer)) {
        if (OsPathMakeDIR(PathBuffer)) {
            printf(MESSAGE_ERROR "为[anyf]文件创建目录失败\n");
            exit(EXIT_CODE_FAILURE);
        }
    } else if (!OsPathIsDirectory(PathBuffer)) {
        printf(MESSAGE_ERROR "父目录已被文件占用，无法创建[anyf]文件\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!(AnyfHandle = fopen(AnyfPathCopied, "wb"))) {
        printf(MESSAGE_ERROR "[anyf]文件创建失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (fwrite(&DEFAULT_HEAD, sizeof(DEFAULT_HEAD), 1, AnyfHandle) != 1) {
        fclose(AnyfHandle);
        remove(AnyfPath);
        PRINT_ERROR_AND_ABORT("写入[anyf]文件头信息失败");
    }
    // 此时文件指针已经在文件末尾
    if (AnyfType = malloc(sizeof(ANYF_T))) {
        AnyfType->head = DEFAULT_HEAD;
        AnyfType->start = 0LL;
        AnyfType->sheet = NULL;
        AnyfType->cells = 0LL;
        AnyfType->path = AnyfPathCopied;
        AnyfType->handle = AnyfHandle;
        return AnyfType;
    } else {
        fclose(AnyfHandle);
        remove(AnyfPath);
        PRINT_ERROR_AND_ABORT("为文件信息结构体分配内存失败");
    }
}

// 打开已存在的[anyf]文件
ANYF_T *AnyfOpen(const char *fp_path) {
    ANYF_T *AnyfType;               // [anyf]文件信息结构体
    HEAD_T HeadTemp;                // 临时[anyf]文件头
    INFO_T *SubFileSheet;           // 子文件信息表
    FILE *AnyfHandle;               // [anyf]文件二进制流
    char PathBuffer[PATH_MAX_SIZE]; // 绝对路径及父目录缓冲
    char *AnyfPathCopied;           // 拷贝路径用于结构体
    int64_t CellsCount = 0LL;       // 子文件信息表容量
    if (OsPathAbsolutePath(PathBuffer, PATH_MAX_SIZE, fp_path)) {
        printf(MESSAGE_ERROR "无法获取[anyf]文件绝对路径：%s\n", fp_path);
        exit(EXIT_CODE_FAILURE);
    }
    printf(MESSAGE_INFO "打开[anyf]文件：%s\n", PathBuffer);
    AnyfPathCopied = malloc(strlen(PathBuffer) + 1ULL);
    if (!AnyfPathCopied) {
        PRINT_ERROR_AND_ABORT("为[anyf]文件文件名分配内存失败");
    }
    strcpy(AnyfPathCopied, PathBuffer);
    if (!OsPathExists(AnyfPathCopied)) {
        printf(MESSAGE_ERROR "指定的文件路径不存在\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathIsFile(AnyfPathCopied)) {
        if (OsPathLastState()) {
            printf(MESSAGE_ERROR "获取[anyf]文件路径属性失败\n");
            exit(EXIT_CODE_FAILURE);
        } else {
            printf(MESSAGE_ERROR "此路径不是一个文件路径\n");
            exit(EXIT_CODE_FAILURE);
        }
    }
    if (!(AnyfHandle = fopen(AnyfPathCopied, "r+b"))) {
        printf(MESSAGE_ERROR "[anyf]文件打开失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (fread(&HeadTemp, sizeof(HeadTemp), 1, AnyfHandle) != 1) {
        PRINT_ERROR_AND_ABORT("读取[anyf]文件头失败");
    }
    if (memcmp(DEFAULT_HEAD.id, HeadTemp.id, sizeof(DEFAULT_HEAD.id))) {
        printf(MESSAGE_ERROR "此文件不是一个[anyf]文件\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (HeadTemp.count > 0)
        CellsCount = HeadTemp.count;
    else
        CellsCount = 1LL;
    SubFileSheet = malloc((size_t)CellsCount * sizeof(INFO_T));
    if (!SubFileSheet) {
        PRINT_ERROR_AND_ABORT("为子文件信息表分配内存失败");
    }
    if (AnyfSeek(AnyfHandle, SUBDATA_OFFSET, SEEK_SET)) {
        PRINT_ERROR_AND_ABORT("移动文件指针到数据块起始位置失败");
    }
    for (int64_t i = 0; i < HeadTemp.count; ++i) {
        if ((SubFileSheet[i].offset = AnyfTell(AnyfHandle)) < 0LL) {
            PRINT_ERROR_AND_ABORT("获取当前子文件信息起始偏移量失败");
        }
        if (fread(&SubFileSheet[i].fsize, FSIZE_FNLEN_SIZE, 1, AnyfHandle) != 1) {
            PRINT_ERROR_AND_ABORT("读取子文件属性失败");
        }
        if (SubFileSheet[i].fnlen > PATH_MAX_SIZE) {
            PRINT_ERROR_AND_ABORT("读取到的子文件名长度异常");
        }
        if (fread(SubFileSheet[i].fname, SubFileSheet[i].fnlen, 1, AnyfHandle) != 1) {
            PRINT_ERROR_AND_ABORT("从[anyf]文件读取子文件名失败");
        }
#ifdef _WIN32
        StringUTF8ToANSI(SubFileSheet[i].fname, PMS, SubFileSheet[i].fname);
#endif // _WIN32
       // 遇到目录(大小是-1)或文件大小为0时没有数据块不需要移动文件指针
        if (SubFileSheet[i].fsize <= 0)
            continue;
        if (AnyfSeek(AnyfHandle, SubFileSheet[i].fsize, SEEK_CUR)) {
            PRINT_ERROR_AND_ABORT("移动文件指针至下一个位置失败");
        }
    }
    AnyfSeek(AnyfHandle, 0, SEEK_END); // 默认文件指针在末尾
    if (AnyfType = malloc(sizeof(ANYF_T))) {
        AnyfType->head = HeadTemp;
        AnyfType->start = 0LL;
        AnyfType->sheet = SubFileSheet;
        AnyfType->cells = CellsCount;
        AnyfType->path = AnyfPathCopied;
        AnyfType->handle = AnyfHandle;
        return AnyfType;
    } else {
        free(SubFileSheet), free(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("为[anyf]文件信息结构体分配内存失败");
    }
}

// 将目标打包进已创建的空[anyf]文件
ANYF_T *AnyfPack(const char *ToBePacked, bool Recursion, ANYF_T *AnyfType, bool Append) {
    // 如果packto是目录，则此变量用于存放其父目录
    char *ParentDIR;
    // 存放绝对路径用于比较是否同一文件
    static char AbsPathBuffer1[PATH_MAX_SIZE]; // [anyf]文件
    static char AbsPathBuffer2[PATH_MAX_SIZE]; // 子文件
#ifdef _WIN32
    static char NormcasedBuffer[PATH_MAX_SIZE]; // WIN平台比较路径是否相同需要先转全小写
#endif
    // 收集路径的 scanlist 扫描器
    SCANNER_T *PathScanner;
    FILE *SubFileStream; // 打开子文件共用指针
    // 用于临时读写文件大小、文件名长度、文件名，也用于更新[anyf]文件结构体的子文件信息表
    INFO_T InfoTemp;
    BUFFER_T *BufferRW; // 文件读写缓冲区
    if (!ToBePacked) {
        PRINT_ERROR_AND_ABORT("打包目标路径是空指针");
    } else if (!*ToBePacked) {
        PRINT_ERROR_AND_ABORT("打包目标路径是空字符串");
    }
    if (!OsPathExists(ToBePacked)) {
        printf(MESSAGE_ERROR "目标文件或目录不存在：%s\n", ToBePacked);
        exit(EXIT_CODE_FAILURE);
    }
    if (AnyfType->head.count > 0LL && !Append) {
        printf(MESSAGE_WARN "此[anyf]文件已包含%" I64_SPECIFIER "个子文件，但未指定追加打包\n", AnyfType->head.count);
        exit(EXIT_CODE_FAILURE);
    }
    strcpy(AbsPathBuffer1, AnyfType->path);
    if (!OsPathNormpath(OsPathNormcase(AbsPathBuffer1), PATH_MAX_SIZE)) {
        WHETHER_CLOSE_REMOVE(AnyfType);
        PRINT_ERROR_AND_ABORT("获取标准形式路径失败");
    }
    // 初始缓冲区大小 BUF_SIZE_L 字节
    if (BufferRW = malloc(sizeof(BUFFER_T) + BUF_SIZE_L)) {
        BufferRW->size = BUF_SIZE_L;
    } else {
        WHETHER_CLOSE_REMOVE(AnyfType);
        PRINT_ERROR_AND_ABORT("为文件读写缓冲区分配初始内存失败");
    }
    if (OsPathIsFile(ToBePacked)) {
        printf(MESSAGE_INFO "打包：%s\n", ToBePacked);
        if (OsPathAbsolutePath(AbsPathBuffer2, PATH_MAX_SIZE, ToBePacked)) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("获取子文件绝对路径失败");
        }
#ifndef _WIN32
        if (!strcmp(AbsPathBuffer1, AbsPathBuffer2))
#else
        strcpy(NormcasedBuffer, AbsPathBuffer2);
        if (!strcmp(AbsPathBuffer1, OsPathNormcase(NormcasedBuffer)))
#endif // _WIN32
        {
            WHETHER_CLOSE_REMOVE(AnyfType);
            printf(MESSAGE_ERROR "退出：此文件是正在创建的[anyf]文件\n");
            exit(EXIT_CODE_SUCCESS);
        }
        if (!OsPathBaseName(InfoTemp.fname, PATH_MAX_SIZE, AbsPathBuffer2)) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            printf(MESSAGE_ERROR "获取子文件名失败：%s\n", AbsPathBuffer2);
            exit(EXIT_CODE_FAILURE);
        }
#ifdef _WIN32
        // WIN平台需要把文件名字符转为UTF8编码的字符串
        StringANSIToUTF8(InfoTemp.fname, PATH_MAX_SIZE, InfoTemp.fname);
#endif // _WIN32
        InfoTemp.fnlen = (int16_t)(strlen(InfoTemp.fname) + 1);
        if (!(SubFileStream = fopen(ToBePacked, "rb"))) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            printf(MESSAGE_ERROR "打开子文件失败：%s\n", ToBePacked);
            exit(EXIT_CODE_FAILURE);
        }
        AnyfSeek(SubFileStream, 0, SEEK_END);
        InfoTemp.fsize = AnyfTell(SubFileStream);
        rewind(SubFileStream); // 子文件读取大小后文件指针移回开头备用
        // 无需将[anyf]文件指针移至末尾，因为 AnyfOpen 或 AnyfMake 函数已将其移至末尾
        if ((InfoTemp.offset = AnyfTell(AnyfType->handle)) < 0LL) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("获取当前子文件信息起始偏移量失败");
        }
        // 将 INFO_T 结构体从第二个成员 fsize 开始写入文件，第一个成员 offset 不需要保存到文件
        if (fwrite(&InfoTemp.fsize, FSIZE_FNLEN_SIZE + InfoTemp.fnlen, 1, AnyfType->handle) != 1) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("写入子文件属性失败");
        }
        if (InfoTemp.fsize > BUF_SIZE_U) {
            if (!SubCopyToMain(SubFileStream, AnyfType->handle, &BufferRW)) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("将子文件写入[anyf]文件失败\n");
            }
        } else if (InfoTemp.fsize > 0) {
            if (InfoTemp.fsize > BufferRW->size && !ExpandBUF(&BufferRW, InfoTemp.fsize)) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("扩充文件读写缓冲区空间失败");
            }
            if (fread(BufferRW->fdata, InfoTemp.fsize, 1, SubFileStream) != 1) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("读取子文件失败");
            }
            if (fwrite(BufferRW, InfoTemp.fsize, 1, AnyfType->handle) != 1) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("写入子文件失败");
            }
        }
        fclose(SubFileStream);
        AnyfSeek(AnyfType->handle, AnyfType->start + COUNT_OFFSET, SEEK_SET);
        if (fwrite(&AnyfType->head.count, sizeof(int64_t), 1, AnyfType->handle) != 1) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("更新子文件数量失败");
        }
        AnyfSeek(AnyfType->handle, 0LL, SEEK_END);
        if (AnyfType->cells <= AnyfType->head.count) {
            if (!ExpandBOM(AnyfType, 1ULL)) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("扩充子文件信息表容量失败");
            }
        }
        AnyfType->sheet[AnyfType->head.count++] = InfoTemp;
    } else if (OsPathIsDirectory(ToBePacked)) {
        if (OsPathAbsolutePath(AbsPathBuffer2, PATH_MAX_SIZE, ToBePacked)) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("获取子文件目录绝对路径失败");
        }
        if (ParentDIR = malloc(PATH_MAX_SIZE)) {
            if (!OsPathDirName(ParentDIR, PATH_MAX_SIZE, OsPathNormpath(AbsPathBuffer2, PATH_MAX_SIZE))) {
                WHETHER_CLOSE_REMOVE(AnyfType);
                PRINT_ERROR_AND_ABORT("获取子文件目录的父目录失败");
            }
        } else {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("为子文件父目录缓冲区分配内存失败");
        }
        printf(MESSAGE_INFO "扫描目录...\n");
        if (!(PathScanner = OsPathMakeScanner(0))) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("创建路径扫描器失败");
        }
        if (OsPathScanPath(AbsPathBuffer2, OSPATH_BOTH, Recursion, &PathScanner)) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            printf(MESSAGE_ERROR "扫描目录失败：%s\n", AbsPathBuffer2);
            exit(EXIT_CODE_FAILURE);
        }
        if (!ExpandBOM(AnyfType, PathScanner->count)) {
            WHETHER_CLOSE_REMOVE(AnyfType);
            PRINT_ERROR_AND_ABORT("扩充子文件信息表容量失败");
        }
        for (size_t i = 0; i < PathScanner->count; ++i) {
            printf(MESSAGE_INFO "打包：%s\n", PathScanner->paths[i]);
            if (OsPathIsDirectory(PathScanner->paths[i])) {
                InfoTemp.fsize = DIR_SIZE; // 目录大小定义为DIR_SIZE
                if (OsPathRelativePath(InfoTemp.fname, PATH_MAX_SIZE, PathScanner->paths[i], ParentDIR)) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    printf(MESSAGE_WARN "跳过：获取子目录相对路径失败");
                    continue;
                }
#ifdef _WIN32
                // WIN平台要把字符串转为UTF8编码写入文件
                StringANSIToUTF8(InfoTemp.fname, PATH_MAX_SIZE, InfoTemp.fname);
#endif // _WIN32
                InfoTemp.fnlen = (int16_t)(strlen(InfoTemp.fname) + 1);
                if ((InfoTemp.offset = AnyfTell(AnyfType->handle)) < 0LL) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    printf(MESSAGE_WARN "跳过：获取当前子文件信息起始偏移量失败\n");
                    continue;
                }
                // 按fsize、fnlen类型长度及fnlen值将finfo_tmp的一部分写入[anyf]文件
                if (fwrite(&InfoTemp.fsize, FSIZE_FNLEN_SIZE + InfoTemp.fnlen, 1, AnyfType->handle) != 1) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    } else {
                        AnyfSeek(AnyfType->handle, InfoTemp.offset, SEEK_SET);
                    }
                    printf(MESSAGE_WARN "跳过：写入子文件属性失败\n");
                    continue;
                }
            } else {
#ifndef _WIN32
                if (!strcmp(AbsPathBuffer1, PathScanner->paths[i]))
#else
                strcpy(NormcasedBuffer, PathScanner->paths[i]);
                if (!strcmp(AbsPathBuffer1, OsPathNormcase(NormcasedBuffer)))
#endif // _WIN32
                {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    printf(MESSAGE_WARN "跳过：此文件是正在创建的[anyf]文件\n");
                    continue;
                }
                if (OsPathRelativePath(InfoTemp.fname, PATH_MAX_SIZE, PathScanner->paths[i], ParentDIR)) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    printf(MESSAGE_WARN "跳过：获取子文件相对路径失败\n");
                    continue;
                }
                if (!(SubFileStream = fopen(PathScanner->paths[i], "rb"))) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    printf(MESSAGE_WARN "跳过：子文件打开失败\n");
                    continue;
                }
                AnyfSeek(SubFileStream, 0, SEEK_END);
                InfoTemp.fsize = (int64_t)AnyfTell(SubFileStream);
                // 子文件读取大小后指针移回开头备用
                rewind(SubFileStream);
#ifdef _WIN32 // WIN平台需要将文件名编码转为UTF8保存
                StringANSIToUTF8(InfoTemp.fname, PATH_MAX_SIZE, InfoTemp.fname);
#endif // _WIN32
                InfoTemp.fnlen = (int16_t)(strlen(InfoTemp.fname) + 1);
                if ((InfoTemp.offset = AnyfTell(AnyfType->handle)) < 0LL) {
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    }
                    fclose(SubFileStream);
                    printf(MESSAGE_WARN "跳过：获取[anyf]文件指针位置失败\n");
                    continue;
                }
                if (fwrite(&InfoTemp.fsize, FSIZE_FNLEN_SIZE + InfoTemp.fnlen, 1, AnyfType->handle) != 1) {
                    printf(MESSAGE_WARN "跳过：写入子文件属性失败\n");
                    if (i >= PathScanner->count - 1) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                    } else {
                        AnyfSeek(AnyfType->handle, InfoTemp.offset, SEEK_SET);
                    }
                    fclose(SubFileStream);
                    continue;
                }
                if (InfoTemp.fsize > BUF_SIZE_U) {
                    if (!SubCopyToMain(SubFileStream, AnyfType->handle, &BufferRW)) {
                        if (i >= PathScanner->count - 1) {
                            WHETHER_CLOSE_REMOVE(AnyfType);
                        } else {
                            AnyfSeek(AnyfType->handle, InfoTemp.offset, SEEK_SET);
                        }
                        fclose(SubFileStream);
                        printf(MESSAGE_WARN "跳过：将子文件写入[anyf]文件失败\n");
                        continue;
                    }
                } else if (InfoTemp.fsize > 0) { // 大小等于0的文件无需读写
                    if (InfoTemp.fsize > BufferRW->size && !ExpandBUF(&BufferRW, InfoTemp.fsize)) {
                        WHETHER_CLOSE_REMOVE(AnyfType);
                        PRINT_ERROR_AND_ABORT("扩充文件读写缓冲区空间失败");
                    }
                    if (fread(BufferRW->fdata, InfoTemp.fsize, 1, SubFileStream) != 1) {
                        if (i >= PathScanner->count - 1) {
                            WHETHER_CLOSE_REMOVE(AnyfType);
                        }
                        fclose(SubFileStream);
                        printf(MESSAGE_WARN "跳过：读取子文件失败\n");
                        continue;
                    }
                    if (fwrite(BufferRW->fdata, InfoTemp.fsize, 1, AnyfType->handle) != 1) {
                        if (i >= PathScanner->count - 1) {
                            WHETHER_CLOSE_REMOVE(AnyfType);
                        } else {
                            AnyfSeek(AnyfType->handle, InfoTemp.offset, SEEK_SET);
                        }
                        fclose(SubFileStream);
                        printf(MESSAGE_WARN "跳过：将子文件写入[anyf]文件失败\n");
                        continue;
                    }
                }
                fclose(SubFileStream);
            }
            AnyfType->sheet[AnyfType->head.count++] = InfoTemp;
        }
        OsPathDeleteScanner(PathScanner);
    } else {
        WHETHER_CLOSE_REMOVE(AnyfType);
        printf(MESSAGE_ERROR "路径不是文件也不是目录：%s\n", ToBePacked);
        exit(EXIT_CODE_FAILURE);
    }
    AnyfSeek(AnyfType->handle, AnyfType->start + COUNT_OFFSET, SEEK_SET);
    if (fwrite(&AnyfType->head.count, sizeof(int64_t), 1, AnyfType->handle) != 1) {
        WHETHER_CLOSE_REMOVE(AnyfType);
        PRINT_ERROR_AND_ABORT("更新[anyf]文件中的子文件数量失败");
    }
    if (BufferRW)
        free(BufferRW);
    return AnyfType;
}

// 打印[anyf]文件中的文件列表即其他信息
ANYF_T *AnyfInfo(const char *AnyfPath) {
    int A, B, C;                     // 已打印的(大小、类型、路径)累计字符数
    size_t NameLenTemp;              // 每个fname长度的临时变量
    size_t NameLenMax = 0;           // 长度最大的fname的值
    int16_t *Spec;                   // 指向head中的std，写完显得太长
    int64_t Index;                   // 遍历[anyf]文件中文件总数head.count
    ANYF_T *AnyfType;                // [anyf]文件信息结构体
    char Delimiters1[EQUAL_MAX];     // 打印的子文件列表分隔符共用缓冲区
    char *Delimiters2, *Delimiters3; // 用于将上面缓冲区分离为三个字符串
    if (AnyfIsFakeJPEG(AnyfPath))
        AnyfType = AnyfOpenFakeJPEG(AnyfPath);
    else
        AnyfType = AnyfOpen(AnyfPath);
    Spec = AnyfType->head.std;
    for (Index = 0; Index < AnyfType->head.count; ++Index) {
        NameLenTemp = strlen(AnyfType->sheet[Index].fname);
        if (NameLenMax < NameLenTemp)
            NameLenMax = NameLenTemp;
    }
#ifdef _MSC_VER
    // 打开MSVC编译器printf的%n占位符支持
    _set_printf_count_output(1);
#endif // _MSC_VER
    printf("\n[ANYF]文件格式版本：");
    printf("%hd.%hd.%hd.%hd\t", Spec[0], Spec[1], Spec[2], Spec[3]);
    printf("包含条目总数：");
    printf("%" I64_SPECIFIER "\n\n", AnyfType->head.count);
    printf("%19s%n\t%4s%n\t%s%n\n", "大小", &A, "类型", &B, "文件名", &C);
    if (NameLenMax < (size_t)C - B - 1)
        NameLenMax = (size_t)C - B - 1;
    else if (NameLenMax >= EQUAL_MAX)
        NameLenMax = EQUAL_MAX - 1;
    memset(Delimiters1, '=', EQUAL_MAX);
    Delimiters1[A] = EMPTY_CHAR;
    Delimiters2 = Delimiters1 + A + 1;
    Delimiters1[B] = EMPTY_CHAR;
    Delimiters3 = Delimiters1 + B + 1;
    Delimiters3[NameLenMax] = EMPTY_CHAR;
    printf("%s\t%s\t%s\n", Delimiters1, Delimiters2, Delimiters3);
    for (Index = 0; Index < AnyfType->head.count; ++Index) {
        printf("%19" I64_SPECIFIER "\t%s\t%s\n", AnyfType->sheet[Index].fsize, AnyfType->sheet[Index].fsize < 0 ? "目录" : "文件", AnyfType->sheet[Index].fname);
    }
    printf("\n[ANYF]文件格式版本：");
    printf("%hd.%hd.%hd.%hd\t", Spec[0], Spec[1], Spec[2], Spec[3]);
    printf("包含条目总数：");
    printf("%" I64_SPECIFIER "\n\n", AnyfType->head.count);
    return AnyfType;
}

// 从[anyf]文件中提取子文件
ANYF_T *AnyfExtract(const char *ToExtract, const char *Destination, int Overwrite, ANYF_T *AnyfType) {
    int64_t Index;  // 循环遍历子文件时的下标
    int64_t Offset; // 子文件信息在[anyf]文件中的偏移量
#ifdef _WIN32
    static char NormcasedBuffer1[PATH_MAX_SIZE];
    static char NormcasedBuffer2[PATH_MAX_SIZE];
#endif
    static char SubFilePathBuffer[PATH_MAX_SIZE];
    static char SubFilePardirBuffer[PATH_MAX_SIZE];
    BUFFER_T *BufferRW;      // 从[anyf]文件提取到子文件时的读写缓冲区
    FILE *EachSubFileHandle; // 创建子文件时每个子文件的二进制文件流句柄
    if (BufferRW = malloc(sizeof(BUFFER_T) + BUF_SIZE_L)) {
        BufferRW->size = BUF_SIZE_L;
    } else {
        PRINT_ERROR_AND_ABORT("为文件读写缓冲区分配内存失败");
    }
    if (!Destination || !*Destination)
        Destination = PATH_CDIRS;
    else {
        if (!OsPathExists(Destination)) {
            if (OsPathLastState()) {
                fprintf(stderr, MESSAGE_ERROR "获取路径属性失败：%s\n", Destination);
                exit(EXIT_CODE_FAILURE);
            }
            if (OsPathMakeDIR(Destination)) {
                fprintf(stderr, MESSAGE_ERROR "创建目录失败：%s\n", Destination);
                exit(EXIT_CODE_FAILURE);
            }
        } else if (!OsPathIsDirectory(Destination)) {
            fprintf(stderr, MESSAGE_ERROR "保存目录已被文件名占用：%s\n", Destination);
            exit(EXIT_CODE_FAILURE);
        }
    }
#ifdef _WIN32
    if (ToExtract) {
        strcpy(NormcasedBuffer1, ToExtract);
        OsPathNormcase(NormcasedBuffer1);
    }
#endif
    for (Index = 0; Index < AnyfType->head.count; ++Index) {
        if (ToExtract) {
#ifdef _WIN32
            strcpy(NormcasedBuffer2, AnyfType->sheet[Index].fname);
            if (strcmp(NormcasedBuffer1, OsPathNormcase(NormcasedBuffer2)))
                continue;
#else
            if (strcmp(ToExtract, AnyfType->sheet[Index].fname))
                continue;
#endif
        }
        printf(MESSAGE_INFO "提取：%s\n", AnyfType->sheet[Index].fname);
        if (OsPathJoinPath(SubFilePathBuffer, PATH_MAX_SIZE, 2, Destination, AnyfType->sheet[Index].fname)) {
            printf(MESSAGE_WARN "跳过：拼接子文件完整路径失败\n");
            continue;
        }
        if (AnyfType->sheet[Index].fsize < 0) {
            if (OsPathExists(SubFilePathBuffer)) {
                if (OsPathIsDirectory(AnyfType->sheet[Index].fname))
                    continue;
                printf(MESSAGE_WARN "跳过：目录名称已被文件占用s\n");
                continue;
            }
            if (OsPathMakeDIR(SubFilePathBuffer)) {
                printf(MESSAGE_WARN "跳过：无法在此位置创建目录\n");
                continue;
            }
        } else {
            if (OsPathExists(SubFilePathBuffer)) {
                if (OsPathIsDirectory(SubFilePathBuffer)) {
                    printf(MESSAGE_WARN "跳过：文件路径已被目录占用：%s\n", SubFilePathBuffer);
                    continue;
                }
                if (!Overwrite) {
                    printf(MESSAGE_WARN "跳过：文件已存在且未指定覆盖：%s\n", SubFilePathBuffer);
                    continue;
                }
            }
            if (!OsPathDirName(SubFilePardirBuffer, PATH_MAX_SIZE, SubFilePathBuffer)) {
                printf(MESSAGE_WARN "跳过：获取父级路径失败\n");
                continue;
            }
            if (OsPathExists(SubFilePardirBuffer)) {
                if (OsPathIsFile(SubFilePardirBuffer)) {
                    printf(MESSAGE_WARN "跳过：目录路径已被文件占用：%s\n", SubFilePardirBuffer);
                    continue;
                }
            } else {
                if (OsPathMakeDIR(SubFilePardirBuffer)) {
                    printf(MESSAGE_WARN "跳过：目录创建失败：%s\n", SubFilePardirBuffer);
                }
            }
            if (!(EachSubFileHandle = fopen(SubFilePathBuffer, "wb"))) {
                printf(MESSAGE_WARN "跳过：子文件创建失败：%s\n", SubFilePathBuffer);
                continue;
            }
            Offset = AnyfType->sheet[Index].offset + FSIZE_FNLEN_SIZE + AnyfType->sheet[Index].fnlen;
            if (AnyfType->sheet[Index].fsize > BUF_SIZE_U) {
                if (!MainCopyToSub(AnyfType->handle, Offset, AnyfType->sheet[Index].fsize, EachSubFileHandle, &BufferRW)) {
                    printf(MESSAGE_WARN "跳过：写入子文件数据失败：%s\n", SubFilePathBuffer);
                    continue;
                }
            } else if (AnyfType->sheet[Index].fsize > 0) {
                if (BufferRW->size < AnyfType->sheet[Index].fsize) {
                    if (!ExpandBUF(&BufferRW, AnyfType->sheet[Index].fsize)) {
                        PRINT_ERROR_AND_ABORT("扩充文件读写缓冲区失败");
                    }
                }
                if (AnyfSeek(AnyfType->handle, Offset, SEEK_SET)) {
                    printf(MESSAGE_WARN "跳过：移动[anyf]文件指针失败\n");
                    continue;
                }
                if (fread(BufferRW->fdata, AnyfType->sheet[Index].fsize, 1, AnyfType->handle) != 1) {
                    printf(MESSAGE_WARN "跳过：读取子文件数据失败");
                    continue;
                }
                if (fwrite(BufferRW->fdata, AnyfType->sheet[Index].fsize, 1, EachSubFileHandle) != 1) {
                    printf(MESSAGE_WARN "跳过：写入子文件数据失败");
                    continue;
                }
            }
            fclose(EachSubFileHandle);
        }
    }
    if (BufferRW)
        free(BufferRW);
    return AnyfType;
}

// 创建空的伪装的JPEG文件
ANYF_T *AnyfMakeFakeJPEG(const char *AnyfPath, const char *JPEGPath, bool Overwrite) {
    ANYF_T *AnyfType;               // [anyf]文件信息结构体
    int64_t JPEGNetSize;            // JPEG文件净大小
    int64_t FakeJPEGSize;           // JPEG文件的总大小
    BUFFER_T *BufferRW;             // 文件读写缓冲区
    FILE *AnyfHandle;               // [anyf]文件文件流
    FILE *JPEGHandle;               // JPEG文件文件流
    char *AnyfPathCopied;           // 复制的文件路径
    char PathBuffer[PATH_MAX_SIZE]; // 绝对路径及父目录缓冲
    if (OsPathAbsolutePath(PathBuffer, PATH_MAX_SIZE, AnyfPath)) {
        printf(MESSAGE_ERROR "无法获取[anyf]文件绝对路径：%s\n", AnyfPath);
        exit(EXIT_CODE_FAILURE);
    }
    printf(MESSAGE_INFO "创建[anyf]文件：%s\n", PathBuffer);
    AnyfPathCopied = malloc(strlen(PathBuffer) + 1ULL);
    if (!AnyfPathCopied) {
        PRINT_ERROR_AND_ABORT("为[anyf]文件路径数组分配内存失败");
    }
    strcpy(AnyfPathCopied, PathBuffer);
    if (OsPathExists(AnyfPathCopied)) {
        if (OsPathIsDirectory(AnyfPathCopied)) {
            printf(MESSAGE_ERROR "此位置已存在同名目录\n");
            exit(EXIT_CODE_FAILURE);
        } else if (OsPathLastState()) {
            printf(MESSAGE_ERROR "获取路径属性失败\n");
            exit(EXIT_CODE_FAILURE);
        } else if (!Overwrite) {
            printf(MESSAGE_ERROR "已存在同名文件但未指定覆盖\n");
            exit(EXIT_CODE_FAILURE);
        }
    } else if (OsPathLastState()) {
        printf(MESSAGE_ERROR "无法检查此路径是否存在\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathDirName(PathBuffer, PATH_MAX_SIZE, PathBuffer)) {
        printf(MESSAGE_ERROR "获取[anyf]文件的父目录路径失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathExists(PathBuffer)) {
        if (OsPathMakeDIR(PathBuffer)) {
            printf(MESSAGE_ERROR "为[anyf]文件创建目录失败\n");
            exit(EXIT_CODE_FAILURE);
        }
    } else if (!OsPathIsDirectory(PathBuffer)) {
        printf(MESSAGE_ERROR "父目录名已被文件名占用：%s\n", PathBuffer);
        exit(EXIT_CODE_FAILURE);
    }
    if (!OsPathIsFile(JPEGPath)) {
        printf(MESSAGE_ERROR "指定的图像路径不是一个文件或不存在：%s\n", JPEGPath);
        exit(EXIT_CODE_FAILURE);
    }
    if (!(AnyfHandle = fopen(AnyfPathCopied, "wb"))) {
        PRINT_ERROR_AND_ABORT("[anyf]文件创建失败");
    }
    if (!(JPEGHandle = fopen(JPEGPath, "rb"))) {
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("打开JPEG图像文件失败");
    }
    if (AnyfSeek(JPEGHandle, 0LL, SEEK_END)) {
        fclose(JPEGHandle);
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("移动JPEG文件指针失败");
    }
    if ((FakeJPEGSize = AnyfTell(JPEGHandle)) < 0LL) {
        fclose(JPEGHandle);
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("读取JPEG文件大小失败");
    }
    if (BufferRW = malloc(sizeof(BUFFER_T) + BUF_SIZE_L)) {
        BufferRW->size = BUF_SIZE_L;
    } else {
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("为文件读写缓冲区分配内存失败");
    }
    JPEGNetSize = RealSizeOfJPEG(JPEGHandle, FakeJPEGSize, &BufferRW);
    rewind(JPEGHandle);
    if (JPEGNetSize == JPEG_INVALID) {
        printf(MESSAGE_WARN "无效的JPEG图像：%s\n", JPEGPath);
        exit(EXIT_CODE_FAILURE);
    } else if (JPEGNetSize == JPEG_ERROR) {
        PRINT_ERROR_AND_ABORT("验证JPEG图像过程中发生错误");
    }
    if (FakeJPEGSize > BUF_SIZE_U) {
        if (!SubCopyToMain(JPEGHandle, AnyfHandle, &BufferRW)) {
            fclose(JPEGHandle);
            fclose(AnyfHandle), remove(AnyfPathCopied);
            PRINT_ERROR_AND_ABORT("复制JPEG文件失败");
        }
    } else {
        if (FakeJPEGSize > BufferRW->size) {
            if (!ExpandBUF(&BufferRW, FakeJPEGSize)) {
                fclose(JPEGHandle);
                fclose(AnyfHandle), remove(AnyfPathCopied);
                PRINT_ERROR_AND_ABORT("为文件读写缓冲区扩充内存失败");
            }
        }
        if (fread(BufferRW->fdata, FakeJPEGSize, 1, JPEGHandle) != 1) {
            fclose(JPEGHandle);
            fclose(AnyfHandle), remove(AnyfPathCopied);
            PRINT_ERROR_AND_ABORT("读取JPEG文件失败");
        }
        if (fwrite(BufferRW->fdata, FakeJPEGSize, 1, AnyfHandle) != 1) {
            fclose(JPEGHandle);
            fclose(AnyfHandle), remove(AnyfPathCopied);
            PRINT_ERROR_AND_ABORT("复制JPEG文件到[anyf]文件失败");
        }
    }
    fclose(JPEGHandle);
    if (AnyfSeek(AnyfHandle, JPEGNetSize, SEEK_SET)) {
        PRINT_ERROR_AND_ABORT("移动伪JPEG文件指针失败");
    }
    if (fwrite(&DEFAULT_HEAD, sizeof(HEAD_T), 1, AnyfHandle) != 1) {
        fclose(JPEGHandle);
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("写入[anyf]文件头信息失败");
    }
    // 最后一次写入后[anyf]文件的文件指针已移至末尾不需再移
    if (AnyfType = malloc(sizeof(ANYF_T))) {
        AnyfType->head = DEFAULT_HEAD;
        AnyfType->start = JPEGNetSize;
        AnyfType->sheet = NULL;
        AnyfType->cells = 0LL;
        AnyfType->path = AnyfPathCopied;
        AnyfType->handle = AnyfHandle;
        if (BufferRW)
            free(BufferRW);
        return AnyfType;
    } else {
        fclose(AnyfHandle), remove(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("为[anyf]文件信息结构体分配内存失败");
    }
}

// 打开已存在的伪装的JPEG文件
ANYF_T *AnyfOpenFakeJPEG(const char *FakeJPEGPath) {
    ANYF_T *AnyfType;               // [anyf]文件信息结构体
    HEAD_T HeadTemp;                // 临时[anyf]文件头
    INFO_T *SubFilesBOM;            // 子文件信息表
    FILE *AnyfHandle;               // [anyf]文件流
    char PathBuffer[PATH_MAX_SIZE]; // 绝对路径及父目录缓冲
    char *AnyfPathCopied;           // 拷贝路径用于结构体
    int64_t CellsNum = 0LL;         // 子文件信息表容量
    BUFFER_T *BufferRW;             // 文件读写缓冲区
    int64_t FakeJPEGSize;           // 伪JPEG文件的总大小
    int64_t JPEGNetSize;            // 伪JPEG文件净大小
    if (OsPathAbsolutePath(PathBuffer, PATH_MAX_SIZE, FakeJPEGPath)) {
        printf(MESSAGE_ERROR "无法获取文件绝对路径：%s\n", FakeJPEGPath);
        exit(EXIT_CODE_FAILURE);
    }
    printf(MESSAGE_INFO "打开[anyf]文件：%s\n", PathBuffer);
    AnyfPathCopied = malloc(strlen(PathBuffer) + 1ULL);
    if (!AnyfPathCopied) {
        PRINT_ERROR_AND_ABORT("为[anyf]文件文件名分配内存失败");
    }
    strcpy(AnyfPathCopied, PathBuffer);
    if (!OsPathIsFile(AnyfPathCopied)) {
        if (OsPathLastState()) {
            printf(MESSAGE_ERROR "获取[anyf]文件路径属性失败\n");
            exit(EXIT_CODE_FAILURE);
        } else {
            printf(MESSAGE_ERROR "此路径不是一个文件路径\n");
            exit(EXIT_CODE_FAILURE);
        }
    }
    if (BufferRW = malloc(sizeof(BUFFER_T) + BUF_SIZE_L)) {
        BufferRW->size = BUF_SIZE_L;
    } else {
        PRINT_ERROR_AND_ABORT("为文件读写缓冲区分配内存失败");
    }
    if (!(AnyfHandle = fopen(AnyfPathCopied, "r+b"))) {
        printf(MESSAGE_ERROR "[anyf]文件打开失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (AnyfSeek(AnyfHandle, 0LL, SEEK_END)) {
        PRINT_ERROR_AND_ABORT("无法移动文件指针至末尾");
    }
    if ((FakeJPEGSize = AnyfTell(AnyfHandle)) < 0) {
        PRINT_ERROR_AND_ABORT("获取伪图文件大小失败");
    }
    JPEGNetSize = RealSizeOfJPEG(AnyfHandle, FakeJPEGSize, &BufferRW);
    if (JPEGNetSize == JPEG_INVALID) {
        printf(MESSAGE_WARN "无效的伪JPEG文件\n");
        exit(EXIT_CODE_FAILURE);
    } else if (JPEGNetSize == JPEG_ERROR) {
        printf(MESSAGE_ERROR "查找伪JPEG文件结束点时出错\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (AnyfSeek(AnyfHandle, JPEGNetSize, SEEK_SET)) {
        printf(MESSAGE_ERROR "移动文件指针失败\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (fread(&HeadTemp, sizeof(HeadTemp), 1, AnyfHandle) != 1) {
        PRINT_ERROR_AND_ABORT("读取[anyf]文件头失败");
    }
    if (memcmp(DEFAULT_HEAD.id, HeadTemp.id, sizeof(DEFAULT_HEAD.id))) {
        printf(MESSAGE_ERROR "此JPEG文件不包含[anyf]文件\n");
        exit(EXIT_CODE_FAILURE);
    }
    if (HeadTemp.count > 0)
        CellsNum = HeadTemp.count;
    else
        CellsNum = 1LL;
    SubFilesBOM = malloc((size_t)CellsNum * sizeof(INFO_T));
    if (!SubFilesBOM) {
        PRINT_ERROR_AND_ABORT("为子文件信息表分配内存失败");
    }
    for (int64_t i = 0; i < HeadTemp.count; ++i) {
        if ((SubFilesBOM[i].offset = AnyfTell(AnyfHandle)) < 0LL) {
            PRINT_ERROR_AND_ABORT("获取当前子文件信息起始偏移量失败");
        }
        if (fread(&SubFilesBOM[i].fsize, FSIZE_FNLEN_SIZE, 1, AnyfHandle) != 1) {
            PRINT_ERROR_AND_ABORT("读取子文件属性失败");
        }
        if (SubFilesBOM[i].fnlen > PATH_MAX_SIZE) {
            PRINT_ERROR_AND_ABORT("读取到的子文件名长度异常");
        }
        if (fread(SubFilesBOM[i].fname, SubFilesBOM[i].fnlen, 1, AnyfHandle) != 1) {
            PRINT_ERROR_AND_ABORT("从[anyf]文件读取子文件名失败");
        }
#ifdef _WIN32
        StringUTF8ToANSI(SubFilesBOM[i].fname, PMS, SubFilesBOM[i].fname);
#endif // _WIN32
       // 遇到目录(大小是-1)或文件大小为0时没有数据块不需要移动文件指针
        if (SubFilesBOM[i].fsize <= 0)
            continue;
        if (AnyfSeek(AnyfHandle, SubFilesBOM[i].fsize, SEEK_CUR)) {
            PRINT_ERROR_AND_ABORT("移动文件指针至下一个位置失败");
        }
    }
    // 默认将文件指针置于末尾
    AnyfSeek(AnyfHandle, 0, SEEK_END);
    if (AnyfType = malloc(sizeof(ANYF_T))) {
        AnyfType->head = HeadTemp;
        AnyfType->start = JPEGNetSize;
        AnyfType->sheet = SubFilesBOM;
        AnyfType->cells = CellsNum;
        AnyfType->path = AnyfPathCopied;
        AnyfType->handle = AnyfHandle;
        return AnyfType;
    } else {
        free(SubFilesBOM), free(AnyfPathCopied);
        PRINT_ERROR_AND_ABORT("为[anyf]文件信息结构体分配内存失败");
    }
}
