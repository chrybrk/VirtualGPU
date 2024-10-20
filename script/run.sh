#!/bin/sh

qemu-system-x86_64 	-m 512 \
										-drive file=out/rootfs.ext4,format=raw \
										-initrd out/initramfs.cpio \
										-kernel kernel/bzImage \
										-append "root=/dev/ram0 rw console=ttyS0" \
										-device virtio-gpu \
										-display gtk \
										-virtfs local,path=./shared,mount_tag=hostshare,security_model=none,writeout=immediate
