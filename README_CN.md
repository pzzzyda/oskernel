简体中文 | [English](README.md)

这是一个参考`xv6-riscv`实现的操作系统内核。

要了解`xv6-riscv`的更多信息，请参考`https://github.com/mit-pdos/xv6-riscv`。

## 依赖

- riscv64-unknown-elf-\*
- qemu-system-riscv64

## 编译和运行

运行`make all`或者`make`编译内核。

运行`make qemu`编译内核并在 qmue 中运行。

## 调试

如果想要调试，可以运行`make qemu-gdb`，然后在 vscode 中开启调试。(需要安装 `C\C++`扩展)
