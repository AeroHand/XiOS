cp mp3.img mac_mp3.img
dd if=mac_mp3.img skip=63 of=fs.img bs=512
debugfs -w -f mac_build_img.txt fs.img
dd if=fs.img of=mac_mp3.img seek=63 bs=512
rm fs.img
