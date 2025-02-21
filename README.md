[简体中文](README_CN.md) | English

This is an operating system kernel that references `xv6-riscv` implementation.

For more information about `xv6-riscv`, see `https://github.com/mit-pdos/xv6-riscv`.

## Dependences

- riscv64-unknown-elf-\*
- qemu-system-riscv64

## Compile and Run

Run `make all` or `make` to compile the kernel.

Run `make qemu` to compile the kernel and run it in qemu.

## Debug

If you want to debug, maybe you can run `make qemu-gdb` and then
turn on debugging in vscode. (The `C\C++` extension needs to be installed)
