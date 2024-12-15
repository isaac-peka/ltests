KVERSION=$(shell uname -r)

obj-m += km_vmap.o
MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all: debug user install

release:
	make -C ../../linux M=${PWD} SUBDIRS=${PWD} modules

debug:
	make -C ../../linux M=${PWD} SUBDIRS=${PWD} modules EXTRA_CFLAGS="$(DEBUG_CFLAGS)"

clean:
	make -C ../../linux M=${PWD} SUBDIRS=${PWD} clean

user: km_vmap_user.c
	gcc -static -o km_vmap_user ./km_vmap_user.c

install: km_vmap_user km_vmap.ko
	cp ./km_vmap_user ./km_vmap.ko ../../home
