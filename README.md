# 名称：anyf

一个用于打包/解包零散文件/目录的程序，也可以将文件/目录打包并伪装成 JPEG 文件/解伪装。打包文件/目录时会保存原始目录结构，解包时创建与原始目录结构一致的新目录。打包后可单独解包其中一个文件，也可全部解包

<br>

# 说明

此仓库是本人仓库 [fapck-github](https://github.com/hrpzcf/fpack) 或 [fpack-gitee](https://gitee.com/hrpzcf/fpack) 的副本，是为了程序改名而创建的，此仓库在`fpack`基础上修复问题和改进，原仓库不再更新。由于此仓库的程序修改了生成的文件的标识符，所以`anyf`和`fapck`无法互相读取对方生成的文件。

<br>

# 目录导航

- [系统支持](#系统支持)
- [编译方法](#编译方法)
- [使用帮助](#使用帮助)

<br>

# 系统支持

#### Linux

- 测试环境：WSL2 Ubuntu x64 20.4.3 LTS；编译器：GCC 9.4.0。

#### Windows

- 测试环境：Windows 10 x64 19044；编译器：MinGW-GCC 12.1.0 / VS2022-MSVC-v142/143；SDK：Windows 10 10.0.19041.0。

#### 其他环境未测试

<br>

# 编译方法

源代码仓库：[GitHub](https://github.com/hrpzcf/anyf) 或 [Gitee](https://gitee.com/hrpzcf/anyf)

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

- Visual Studio 2022：
    1. 安装IDE `Visual Studio 2022` 及 C++ 工作负载，编译工具 `MSVC v143` 或 `v142`，SDK `Windows 10 10.0.19041.0`；
    2. 从源代码仓库下载/克隆fpack项目源代码至本地；
    3. 打开`anyf/msbuild`目录；
    4. 使用`Visual Studio`打开解决方案文件`msbuild.sln`；
    5. 选择上方工具栏`解决方案配置`为`Release`，`x64`；
    6. 选择上方菜单栏`生成`->`生成解决方案`；
    7. 等待编译完成，生成的可执行文件在`anyf/binary`目录下，名为`anyf.exe`。

<br>

# 使用帮助

## 先决条件

1. 将 `anyf` 或 `anyf.exe` 所在目录路径加入系统环境变量 (此步可忽略，但建议加入，否则每次使用 anyf 都需要打开 cmd 再 cd 至 anyf 所在目录，不方便)，一下使用帮助都默认你已经将`anyf`所在目录加入环境变量；
2. 输入命令`anyf help`查看使用帮助。

<br>

## 例 1：将指定目录打包为 ANYF 文件

- 需求：将 `E:\我的音乐`目录内的所有文件（包括其子目录内的文件）打包为名为`E:\music.af`文件。

- 命令：`anyf pack -t E:\我的音乐 -r -f E:\music.af`

- 步骤：

  1. 在`E:\我的音乐`目录打开命令窗口（Windows：选中`我的音乐`文件夹，按住`shift`+鼠标右键，选`在此处打开PowerShell`）
  2. 输入命令`anyf pack -t E:\我的音乐 -r -f E:\music.af`
     - `anyf pack`表示使用`anyf`的`pack`功能（打包功能）
     - `-t E:\我的音乐`表示将要打包的目标设置为`E:\我的音乐`文件夹，由于我们是在`E:\我的音乐`打开命令窗口，所以这个命令也可以简写为`-t .`，`.`代表当前目录，即`E:\我的音乐`
     - `-r`代表递归搜索`-t`选项指定的目录，即递归搜索`E:\我的音乐`文件夹，意思是层层深入该文件夹内的子文件夹。不使用`-r`选项则只收集该文件夹下的一代子文件和文件夹
     - `-f E:\music.af`表示指定生成的 ANYF 文件的路径和文件名为`E:\music.af`，也可以简写为`-f ..\music.af`，`..`表示当前目录`E:\我的音乐`的上一层，即`E:\`
  3. 使用`anyf info -f E:\music.af`查看已打包的 ANYF 文件的信息。

<br>

## 例 2：将指定目录打包并伪装为 JPEG 文件

- 需求：将 `E:\我的音乐`目录内的所有文件（包括其子目录内的文件）打包并伪装为名为`E:\music.jpeg`文件。

- 命令：`anyf fake -t E:\我的音乐 -r -f E:\music.jpeg -j E:\1.jpeg`

- 步骤：

  1. 在`E:\我的音乐`目录打开命令窗口（Windows：选中`我的音乐`文件夹，按住`shift`+鼠标右键，选`在此处打开PowerShell`）
  2. 输入命令`anyf fake -t E:\我的音乐 -r -f E:\music.jpeg -j E:\1.jpeg`
     - `anyf fake`表示使用`anyf`的`fake`功能（打包并伪装功能）
     - `-t E:\我的音乐`表示将要打包的目标设置为`E:\我的音乐`文件夹，由于我们是在`E:\我的音乐`打开命令窗口，所以这个命令也可以简写为`-t .`，`.`代表当前目录，即`E:\我的音乐`
     - `-r`代表递归搜索`-t`选项指定的目录，即递归搜索`E:\我的音乐`文件夹，意思是层层深入该文件夹内的子文件夹。不使用`-r`选项则只收集该文件夹下的一代子文件和文件夹
     - `-f E:\music.jpeg`表示指定生成的伪装为 JPEG 的 ANYF 文件的路径和文件名为`E:\music.jpeg`，也可以简写为`-f ..\music.jpeg`，`..`表示当前目录`E:\我的音乐`的上一层，即`E:\`
  3. 使用`anyf info -f E:\music.jpeg`查看已打包并伪装为 JPEG 的 ANYF 文件的信息。

<br>

## 例 3：从 ANYF 文件或伪装为 JPEG 的 ANYF 文件中提取被打包的文件

- 需求：从前两个例子打包的`E:\music.af`和`E:\music.af`中提取被打包的文件，提取到`E:\已提取`文件夹

- 命令：`anyf extr -f E:\music.af -t E:\已提取`或`anyf extr -f E:\music.jpeg -t E:\已提取`

- 步骤：

  1. 在`E:\我的音乐`目录打开命令窗口（Windows：选中`我的音乐`文件夹，按住`shift`+鼠标右键，选`在此处打开PowerShell`）
  2. 输入命令`anyf extr -f E:\music.af -t E:\已提取`或`anyf extr -f E:\music.jpeg -t E:\已提取`
     - `anyf extr`表示使用`anyf`的`extr`功能（提取文件功能）
     - `-f E:\music.af`或`-f E:\music.jpeg`表示从`E:\music.af`或`E:\music.jpeg`中提取文件
     - `-t E:\已提取`表示将提取的子文件的保存目录设置为`E:\已提取`文件夹

<br>

## 更具体的使用方法可以使用`anyf help`命令查看使用帮助。
