# 构建 QtWebDriver
先仔细阅读官方的说明:
* https://github.com/cisco-open-source/qtwebdriver/wiki/Build-And-Run 这个页面描述了如何构建 QtWebDriver 的库文件
* https://github.com/cisco-open-source/qtwebdriver/wiki/Use-QtWebDriver-to-run-your-application 这个页面描述了如何在我们的程序中加入 QtWebDriver 的代码

## 构建 QtWebDriver 的补充说明
* 本代码仓的最近的6次提交是为了在 Win11（Win10也可以）上面构建 QtWebDriver，基于 VS2015 以上版本。其中有几个关键的改动如下：
  * 更新了`build.bat`来支持 VS2015
  * 更新了一些python代码来支持 python3，也fix了一些C代码的编译错误
  * 写了构建 QtWebDriver 的核心配置文件`wd.gypi`
  * 更新了编译选项配置文件`wd_build_options.gypi`
* 如果要构建不同QT版本（当前是5.8）的QtWebDriver，应该只需要修改`wd.gypi`中的QT路径即可
  * 'QT_INC_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/include',
  * 'QT_BIN_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/bin',
  * 'QT_LIB_PATH': 'C:/Qt/Qt5.8.0/5.8/msvc2015_64/lib',
* 运行`build.bat`即可构建

## 关于构建需要工具的补充说明
* 需要安装python3，我用的是 3.12.3，3.8之后的版本估计都可以
* 安装gyp，先要clone https://chromium.googlesource.com/external/gyp 。然后运行`python .\setup.py install`
  * 安装gyp之前，需要安装setuptools，运行`pip install setuptools`
  * 运行gyp，需要安装six，运行`pip install six`
* 确保 devenv 的路径在系统环境变量中
* Visual Studio 需要安装 v140 build tool chain
* 需要安装 Windows SDK 8.1 https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
