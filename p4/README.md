gcloud compute scp --recurse "/Users/huangjiayi/Documents/003UCI/learn/cs238-os/p4" instance-1:

sudo dd if=/dev/zero of=file bs=4096 count=10000
sudo mkfs.ext4 file
sudo mknod -m0660 /dev/loop24 b 7 11
sudo losetup /dev/loop24 file
lsblk
sudo losetup -a

sudo ./cs238 /dev/loop11
sudo valgrind --leak-check=full ./cs238 /dev/loop11