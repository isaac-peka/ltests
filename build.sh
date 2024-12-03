#!/bin/bash -e

export KERNEL_VERSION=6.11.3
export BUSYBOX_VERSION=1.35.0

#
# linux kernel
#

echo "[+] Downloading kernel..."
wget -q -c https://mirrors.edge.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.gz

[ -e linux-$KERNEL_VERSION ] || tar xzf linux-$KERNEL_VERSION.tar.gz

echo "[+] Building kernel..."
make -C linux-$KERNEL_VERSION defconfig
echo "CONFIG_STRICT_KERNEL_RWX=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_RANDOMIZE_BASE=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_NET_9P=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_NET_9P_DEBUG=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_9P_FS=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_9P_FS_POSIX_ACL=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_9P_FS_SECURITY=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_NET_9P_VIRTIO=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_PCI=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_BLK=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_BLK_SCSI=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_NET=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_CONSOLE=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_HW_RANDOM_VIRTIO=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DRM_VIRTIO_GPU=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_PCI_LEGACY=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_BALLOON=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_VIRTIO_INPUT=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_CRYPTO_DEV_VIRTIO=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_BALLOON_COMPACTION=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_PCI=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_PCI_HOST_GENERIC=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_GDB_SCRIPTS=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_REDUCED=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_SPLIT=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_FS=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_DWARF4=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_BTF=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_FRAME_POINTER=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_REDUCED=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DEBUG_INFO_COMPRESSED_NONE=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DRM_BOCHS=m" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_IDE=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_ANDROID_BINDER_IPC=y" >>  linux-$KERNEL_VERSION/.config
echo "CONFIG_ANDROID_BINDERFS=n" >>  linux-$KERNEL_VERSION/.config
echo "CONFIG_ANDROID_BINDER_DEVICES=\"binder\"" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_ANDROID_BINDER_IPC_SELFTEST=n" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DMABUF_HEAPS=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_DMABUF_HEAPS_SYSTEM=y" >> linux-$KERNEL_VERSION/.config
echo "CONFIG_KASLR=y" >> linux-$KERNEL_VERSION/.config

# echo "CONFIG_KSAN=y" >> linux-$KERNEL_VERSION/.config


ln -sf linux linux-$KERNEL_VERSION
make -C linux-$KERNEL_VERSION prepare modules_prepare
make CFLAGS="-O1" -C linux-$KERNEL_VERSION -j$(nproc) bzImage

#
# Busybox
#

echo "[+] Downloading busybox..."
wget -q -c https://busybox.net/downloads/busybox-$BUSYBOX_VERSION.tar.bz2
[ -e busybox-$BUSYBOX_VERSION ] || tar xjf busybox-$BUSYBOX_VERSION.tar.bz2

echo "[+] Building busybox..."
make -C busybox-$BUSYBOX_VERSION defconfig
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/g' busybox-$BUSYBOX_VERSION/.config
sed -i 's/CONFIG_TC=y/CONFIG_TC=n/g' busybox-$BUSYBOX_VERSION/.config
make -C busybox-$BUSYBOX_VERSION -j$(nproc)
make -C busybox-$BUSYBOX_VERSION install

#
# filesystem
#

echo "[+] Building filesystem..."
cd fs
mkdir -p bin sbin etc proc sys usr/bin usr/sbin root home/user
cd ..
cp -a busybox-$BUSYBOX_VERSION/_install/* fs
find modules -name '*.ko' -exec cp \{\} fs/ \;
