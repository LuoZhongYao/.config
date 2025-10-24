/*
 * Copyright (c) 2022 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <endian.h>

struct rtb_patch_entry {
	uint16_t chip_id;
	uint16_t patch_len;
	uint32_t soffset;
	uint32_t svn_ver;
	uint32_t coex_ver;
} __attribute__ ((packed));

struct rtb_patch_hdr {
	uint8_t signature[8];
	uint32_t fw_version;
	uint16_t number_of_patch;
	struct rtb_patch_entry entry[0];
} __attribute__ ((packed));

static const uint8_t rtb_patch_emagic[4] = { 0x51, 0x04, 0xFD, 0x77 };
static const uint8_t rtb_patch_smagic[8] = { 0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68 };

static inline uint16_t get_unaligned_le16(uint8_t * p)
{
	return (uint16_t) (*p) + ((uint16_t) (*(p + 1)) << 8);
}

static inline uint32_t get_unaligned_le32(uint8_t * p)
{
	return (uint32_t) (*p) + ((uint32_t) (*(p + 1)) << 8) +
		((uint32_t) (*(p + 2)) << 16) + ((uint32_t) (*(p + 3)) << 24);
}

static char filename[4096];
int main(int argc, char **argv)
{
	int fd;
	void *addr;
	uint8_t *ci_base;
	uint8_t *pl_base;
	uint8_t *so_base;
	struct stat stbuf;
	struct rtb_patch_hdr *patch;
	struct rtb_patch_entry entry;
	uint16_t number_of_patch;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <patch file>\n", argv[0]);
		return 1;
	}

	if(stat(argv[1], &stbuf)) {
		perror(argv[1]);
		return 1;
	}

	if (0 > (fd = open(argv[1], O_RDONLY))) {
		perror(argv[1]);
		return 1;
	}

	addr = mmap(NULL, stbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == addr) {
		perror("mmap");
		return 1;
	}

	patch = addr;
	if (memcmp(patch->signature, rtb_patch_smagic, sizeof(rtb_patch_smagic))) {
		fprintf(stderr, "invalid signature\n");
		return 1;
	}

	if (memcmp(addr + stbuf.st_size - sizeof(rtb_patch_emagic), rtb_patch_emagic, sizeof(rtb_patch_emagic))) {
		fprintf(stderr, "invalid signature\n");
		return 1;
	}

	number_of_patch = le16toh(patch->number_of_patch);

	ci_base = addr + 14;
	pl_base = ci_base + 2 * patch->number_of_patch;
	so_base = pl_base + 2 * patch->number_of_patch;

	printf("number of patch: %d\n", number_of_patch);
	for (int i = 0; i < number_of_patch; i++) {
		uint16_t chip_id = get_unaligned_le16(ci_base + 2 * i);
		entry.chip_id = chip_id;
		entry.patch_len = get_unaligned_le16(pl_base + 2 * i);
		entry.soffset = get_unaligned_le32(so_base + 4 * i);
		entry.svn_ver = get_unaligned_le32(addr + entry.soffset + entry.patch_len - 8);
		entry.coex_ver = get_unaligned_le32(addr + entry.soffset + entry.patch_len - 12);
		snprintf(filename, sizeof(filename), "%s.%d", argv[1], chip_id);
		int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			perror(filename);
			return 1;
		}
		write(fd, addr + entry.soffset, entry.patch_len);
		close(fd);
	}
	munmap(addr, stbuf.st_size);
	close(fd);
	return 0;
}
