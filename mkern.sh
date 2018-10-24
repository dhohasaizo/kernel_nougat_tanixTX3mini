#!/bin/bash -ex

# Run from top of nougat s905 source

#ROOTFS=$1
ROOTFS="/root/nougat/s905/ramdisk.img"
PREFIX_CROSS_COMPILE=/root/toolchain/bin/aarch64-linux-gnu-

if [ "$ROOTFS" == "" -o ! -f "$ROOTFS" ]; then
    echo "Usage: $0 <ramdisk.img> [m]"
    exit 1
fi

KERNEL_OUT=/root/nougat/s905/out/product/tx3mini
mkdir -p $KERNEL_OUT

if [ ! -f $KERNEL_OUT/.config ]; then
    make O=$KERNEL_OUT tx3mini_defconfig ARCH=arm64 CROSS_COMPILE=$PREFIX_CROSS_COMPILE
fi
#if [ "$2" != "m" ]; then
#    make -C common O=../$KERNEL_OUT ARCH=arm64 -j6 CROSS_COMPILE=$PREFIX_CROSS_COMPILE UIMAGE_LOADADDR=0x1008000
#fi
make O=$KERNEL_OUT ARCH=arm64 -j4 CROSS_COMPILE=$PREFIX_CROSS_COMPILE modules Image.gz

if [ "$2" != "m" ]; then
#    make -C common O=../$KERNEL_OUT kvim.dtd ARCH=arm64 CROSS_COMPILE=$PREFIX_CROSS_COMPILE
    make O=$KERNEL_OUT gxl_p281_1g.dtb gxl_p281_2g.dtb ARCH=arm64 CROSS_COMPILE=$PREFIX_CROSS_COMPILE PARTITION_DTSI=partition_mbox.dtsi
fi

if [ "$2" != "m" ]; then
    /root/nougat/s905/dtbTool -s 2048 -o $KERNEL_OUT/arch/arm64/boot/dtb.img $KERNEL_OUT/arch/arm64/boot/dts/amlogic/
    echo "dtb.img done"
fi

if [ "$2" != "m" ]; then
    /root/nougat/s905/mkbootimg --kernel $KERNEL_OUT/arch/arm64/boot/Image.gz \
        --base 0x0 \
        --cmdline "buildvariant=userdebug" \
        --kernel_offset 0x1080000 \
        --ramdisk ${ROOTFS} \
        --output $KERNEL_OUT/arch/arm64/boot/boot.img
    ls -l $KERNEL_OUT/arch/arm64/boot/boot.img
    echo "boot.img done"
fi