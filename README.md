# 名称：anyf

#### 一个用于打包/解包零散文件/目录的程序，兼顾将文件/目录打包并伪装成JPEG文件/解伪装功能。打包文件/目录时会保存原始目录结构信息，解包时创建与原始目录结构一致的新目录。打包后可单独解包其中一个文件，也可全部解包。

<br>

# 支持的系统

#### Linux
- 测试环境：WSL2 Ubuntu x64 20.4.3 LTS；编译器：GCC 9.3.0。

#### Windows
- 测试环境：Windows 10 x64 19044；编译器：MinGW-GCC 12.1.0 / VS2022-MSVC-v142/143；SDK：Windows 10 10.0.19041.0。

#### 其他环境未测试

<br>

# 源代码编译方法

源代码仓库：
- GITEE: https://gitee.com/hrpzcf/anyf
- GITHUB: https://github.com/hrpzcf/anyf

<br>

linux平台编译方法：
1. 安装CMake，安装GCC编译器；
2. 从源代码仓库下载/克隆fpack项目源代码至本地；
3. 在fpack项目目录打开命令窗口；
4. 输入命令：`cmake -DCMAKE_BUILD_TYPE:STRING=Release -B./build -G "Unix Makefiles"`;
5. 输入命令：`cmake --build ./build`；
6. 等待编译完成，生成的可执行文件在`anyf/binary`目录下，名为`anyf`。

<br>

Windows平台编译方法：
- MinGW-GCC编译器：
  1. 安装CMake，安装GCC编译器(MinGW)；
  2. 从源代码仓库下载/克隆fpack项目源代码至本地；
  3. 在fpack项目目录打开命令窗口；
  4. 输入命令：`cmake -DCMAKE_BUILD_TYPE:STRING=Release -B./build -G "MinGW Makefiles"`;
  5. 输入命令：`cmake --build ./build`；
  6. 等待编译完成，生成的可执行文件在`anyf/binary`目录下，名为`anyf.exe`。

<br>

- Visual Studio：
    1. 安装IDE `Visual Studio 2022` 及 C++ 工作负载，编译工具 `MSVC v142` 或 `v143`；
    2. 从源代码仓库下载/克隆fpack项目源代码至本地；
    3. 打开`anyf/msbuild`目录；
    4. 使用`Visual Studio`打开解决方案文件`msbuild.sln`；
    5. 选择上方工具栏`解决方案配置`为`Release`，`x64`；
    6. 选择上方菜单栏`生成`->`生成解决方案`；
    7. 等待编译完成，生成的可执行文件在`anyf/binary`目录下，名为`anyf.exe`。

<br>

# 使用方法

1. 将 `anyf` 或 `anyf.exe` 所在目录路径加入系统环境变量 PATH (此步可忽略，但建议加入，否则每次使用 anyf 都需要打开 cmd 再 cd 至 anyf 所在目录，不方便)；
2. 输入命令`anyf help`查看帮助。
3. 如果未将 anyf 所在目录加入系统环境变量，则需在命令窗口中 cd 至 `anyf` 所在目录，运行命令 `./anyf help`，如果你使用的是 Windows 的 cmd 则使用 `.\anyf help` 命令。
