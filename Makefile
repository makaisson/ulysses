all: kernel

kernel:
	cd src; make

libc: sys/.libc-configured
	cd sys/libc; make install

sys/.libc-configured: sys/libc
	cd sys/libc; ./configure --prefix=`pwd`/../local
	touch sys/.libc-configured

sys/libc:
	cd sys; git clone git@github.com:evanphx/ulysses-libc.git libc

test: sys/tar_disk


LCC=./local/bin/musl-gcc

sys/tar_disk: sys/test
	cd sys; tar czvf tar_disk test

sys/dyn_tar_disk: sys/test-dyn
	cd sys; cp test-dyn test; tar czvf dyn_tar_disk test link.so; rm test

sys/dash_disk: sys/link.so
	cd sys; tar czvf dash_disk dash link.so

sys/test: sys/test.c
	cd sys; ${LCC} -static -o test test.c

sys/test-dyn: sys/test.c sys/link.so
	cd sys; ${LCC} -Wl,-dynamic-linker -Wl,link.so -o test-dyn test.c

sys/link.so: libc
	cp sys/local/lib/libc.so sys/link.so

run:
	qemu-system-x86_64 -kernel src/kernel -initrd sys/tar_disk -hda scratch/words_disk -append test

rundyn:
	qemu-system-x86_64 -kernel src/kernel -initrd sys/dyn_tar_disk -hda scratch/words_disk -append test

rundash:
	qemu-system-x86_64 -kernel src/kernel -initrd sys/dash_disk -hda scratch/words_disk -append dash

full: libc test all
