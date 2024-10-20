#!/bin/sh

qemu-system-x86_64 \
	-initrd out/initramfs.cpio \
	-kernel kernel/bzImage \
	-append "root=/dev/vda console=ttyS0 nokaslr" \
	-drive format=raw,file=out/rootfs.ext4,if=virtio \
	-m 128M \
	-device e1000,netdev=net0 \
	-netdev user,id=net0,hostfwd=tcp::5555-:22 \
	-device virtio-gpu-gl \
	-display gtk,gl=core \
	-virtfs local,path=./shared,mount_tag=hostshare,security_model=none,writeout=immediate
