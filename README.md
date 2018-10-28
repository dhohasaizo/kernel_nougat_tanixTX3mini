How to build kernel:

- export your toolchain
  EXPORT CROSS_COMPILE=
- cd source
- make ARCH=arm64
- make tx3mini_defconfig
- make -j{usage cpu}  ,example: make -j4

alternatif build:
- cd source
- ./mkern.sh
alternatif build, setup your souce and toolchain in the script mkern.sh

HAPPY BUILD
