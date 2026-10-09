#!/bin/sh
mount -t 9p -o trans=virtio hostshare /mnt
/mnt/prisc_x_linux /mnt/test_linux.pal
poweroff -f
