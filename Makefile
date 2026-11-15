# Makefile - Tugas 4 Filesystem dan I/O (Varian 2: fsync vs fdatasync, KVL)
#
#   make              kompilasi workload dan seluruh program varian
#   make run          jalankan workload (ubah argumen: make run RUN_ARGS="...")
#   make clean        hapus hasil kompilasi (folder bin/)
#
# Ubah nomor kelompok dan kelas di sini atau lewat perintah:
#   make KELOMPOK=01 KELAS=A
# Setelah mengganti KELOMPOK/KELAS, jalankan "make clean" dulu.

KELOMPOK ?= 08
KELAS    ?= A

CC       = gcc
CFLAGS   = -std=gnu11 -Wall -Wextra -O2 -g \
           -DGROUP_ID=\"$(KELOMPOK)\" -DKELAS_ID=\"$(KELAS)\"
DEPFLAGS = -MMD -MP
LDLIBS   =
BIN_DIR  = bin

RUN_ARGS ?= --dir . --size 64 --bs 4 --mode seqwrite

# Program yang dibuat. Yang sumbernya belum ada dilewati (dengan peringatan),
# jadi Makefile ini bisa dipakai sejak awal pengerjaan.
WANTED       := workload durability_bench ext4_inode
PROGRAMS     := $(foreach p,$(WANTED),$(if $(wildcard $(p).c),$(p)))
PROGRAM_BINS := $(PROGRAMS:%=$(BIN_DIR)/%)
$(foreach p,$(WANTED),$(if $(wildcard $(p).c),,$(warning $(p).c belum ada - dilewati)))

COMMON_SRC := iolib.c banner.c
COMMON_OBJ := $(COMMON_SRC:%.c=$(BIN_DIR)/%.o)

.PHONY: all run clean
.SECONDARY:

all: $(COMMON_OBJ) $(PROGRAM_BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%.o: %.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(BIN_DIR)/%.o $(COMMON_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

run: all
	@test -x $(BIN_DIR)/workload || { echo "workload belum ada: buat workload.c dulu"; exit 1; }
	./$(BIN_DIR)/workload $(RUN_ARGS)

clean:
	rm -rf $(BIN_DIR)

-include $(wildcard $(BIN_DIR)/*.d)
