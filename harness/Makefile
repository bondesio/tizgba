# Host harness: ../app/gba_main.c and ../app/filters.c compiled unchanged against
# fake EFL / library / mGBA bridge. -I ../app/include is NOT used: host headers.
CC ?= gcc
CFLAGS := -w -std=gnu11 -O1 -g -pthread -DTIZGBA_DATA_DIR='"out/data"' -DTIZGBA_DEV_INPUT='"out/nodev"' -DTIZGBA_SYS_INPUT='"out/nosys"' -I ../app
.PHONY: ui-tests clean
ui-tests: build/ui_test
	./build/ui_test
build/ui_test: ui_test.c ../app/gba_main.c ../app/filters.c ../app/perf_overlay.h fake_efl.h
	mkdir -p build
	$(CC) $(CFLAGS) -Dmain=tizgba_app_main -c ../app/gba_main.c -o build/gba_main.o
	$(CC) $(CFLAGS) -c ../app/filters.c -o build/filters.o
	$(CC) $(CFLAGS) -c ui_test.c -o build/ui_test.o
	$(CC) -pthread -o $@ build/ui_test.o build/gba_main.o build/filters.o
clean:
	rm -rf build out
