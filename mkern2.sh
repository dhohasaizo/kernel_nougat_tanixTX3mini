#!/bin/bash -ex

# Run from top of nougat s905 source

PREFIX_CROSS_COMPILE=/root/toolchain/bin/aarch64-linux-gnu-

KERNEL_OUT=/root/nougat/s905/out/product/tx3mini
mkdir -p $KERNEL_OUT

if [ "$2" != "m" ]; then
make O=$KERNEL_OUT gxl_p281_1g.dtb gxl_p281_2g.dtb ARCH=arm64 CROSS_COMPILE=$PREFIX_CROSS_COMPILE
fi

if [ "$2" != "m" ]; then
    /root/nougat/s905/dtbTool -s 2048 -o $KERNEL_OUT/arch/arm64/boot/dtb.img $KERNEL_OUT/arch/arm64/boot/dts/amlogic/
    echo "dtb.img done"
fi
