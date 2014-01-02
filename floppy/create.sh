dd bs=512 count=2880 if=/dev/zero of=blank_floppy.img
dd if=./filesys_img count=2880 bs=512 of=floppy.img
