## 编译方法 

<div align="right">
  <a href="../zh_TW/Compile.md">繁體中文</a> | <strong>简体中文</strong> | <a href="../en_US/Compile.md">English</a>
</div>
<br>

Linux 平台:
```shell
mkdir build
cd build
cmake ..
make
```

Windows 平台:
```shell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

使用 xmake 构建:
```shell
xmake
```
