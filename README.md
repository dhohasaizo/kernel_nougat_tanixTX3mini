How to build kernel:
   **Image.gz**
- export your toolchain
  EXPORT CROSS_COMPILE={your toolchain}
- cd source
- make ARCH=arm64
- make tx3mini_defconfig
- make -j{usage cpu}  ,example: make -j4

   **dtb**
- make gxl_p281_1g.dtb gxl_p281_2g.dtb

How to make dtb.img:
install on ubuntu device compiler
sudo apt-get install device-tree-compiler
if u success compile .dtb file
- ./dtbTool -s 2048 -o dtb.img arch/arm64/boot/dts/amlogic/

alternatif build:
- cd source
- ./mkern.sh
alternatif build, setup your souce and toolchain in the script mkern.sh

HAPPY BUILD
