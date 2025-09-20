import vfs

# Mount the partition labeled Storage on /flash
mounts = vfs.mount()
for mount in mounts:
    if mount[1] == "/flash":
        del vfs, mounts
        return

from zephyr import FlashArea

for key, value in FlashArea.__dict__.items():
    if key.lower() == 'id_storage':
        bdev = FlashArea(value, 4096)
        try:
            vfs.mount(bdev, "/flash")
        except OSError:
            vfs.VfsLfs2.mkfs(bdev)
            vfs.mount(bdev, "/flash")

del vfs, mounts, key, value, bdev, FlashArea
