#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sparse_header {
  uint32_t	magic;		/* 0xed26ff3a */
  uint16_t	major_version;	/* (0x1) - reject images with higher major versions */
  uint16_t	minor_version;	/* (0x0) - allow images with higer minor versions */
  uint16_t	file_hdr_sz;	/* 28 bytes for first revision of the file format */
  uint16_t	chunk_hdr_sz;	/* 12 bytes for first revision of the file format */
  uint32_t	blk_sz;		/* block size in bytes, must be a multiple of 4 (4096) */
  uint32_t	total_blks;	/* total blocks in the non-sparse output image */
  uint32_t	total_chunks;	/* total chunks in the sparse input image */
  uint32_t	image_checksum; /* CRC32 checksum of the original data, counting "don't care" */
				/* as 0. Standard 802.3 polynomial, use a Public Domain */
				/* table implementation */
} sparse_header_t;

#define SPARSE_HEADER_MAGIC	0xed26ff3a

#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL		0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC32    0xCAC4

typedef struct chunk_header {
  uint16_t	chunk_type;	/* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
  uint16_t	reserved1;
  uint32_t	chunk_sz;	/* in blocks in output image */
  uint32_t	total_sz;	/* in bytes of chunk input file including chunk header and data */
} chunk_header_t;

/* Following a Raw or Fill or CRC32 chunk is data.
 *  For a Raw chunk, it's the data in chunk_sz * blk_sz.
 *  For a Fill chunk, it's 4 bytes of the fill data.
 *  For a CRC32 chunk, it's 4 bytes of CRC32
 */

static int disable_splice = 0;
void nsendfile(int out_fd, int in_fd, size_t count) {
    char buf[1024*1024];
	while(count) {
		ssize_t res = -1;
        if(!disable_splice) {
            res = splice(in_fd, NULL, out_fd, NULL, count, 0);
        }

        if(count > 16*1024 && res < 1024)
            disable_splice = 1;
        if(res==-1) {
            ssize_t sizeToRead = sizeof(buf);
            if(count < sizeToRead) sizeToRead = count;
            res = read(in_fd, buf, sizeToRead);
            if(write(out_fd, buf, res) != res) exit(114);
        }
		if(res == 0 || res == -1) exit(112);
		count -= res;
	}
}

int main() {
	sparse_header_t hdr;
	if(read(0, &hdr, sizeof(hdr)) != sizeof(hdr)) exit(1);
	if(hdr.magic != SPARSE_HEADER_MAGIC) exit(2);
	if(hdr.blk_sz != 4096) exit(6);
	if(hdr.major_version != 1) exit(11);
	if(hdr.minor_version != 0) exit(12);
	if(hdr.file_hdr_sz != 28) exit(13);
	if(hdr.chunk_hdr_sz != 12) exit(14);

	char block[4096];
	for(unsigned i=0; i<hdr.total_chunks; i++) {
		chunk_header_t chunk;
		if(read(0, &chunk, sizeof(chunk)) != sizeof(chunk)) exit(3);
		if(chunk.chunk_type == CHUNK_TYPE_RAW) {
			if(chunk.total_sz != sizeof(chunk_header_t) + (chunk.chunk_sz * hdr.blk_sz)) exit(7);

			nsendfile(1, 0, hdr.blk_sz * chunk.chunk_sz);
		} else if(chunk.chunk_type == CHUNK_TYPE_FILL) {
			if(chunk.total_sz != 4 + sizeof(chunk_header_t)) exit(7);

			uint32_t fill;
			if(read(0, &fill, sizeof(fill)) != sizeof(fill)) exit(5);
			//memset takes a char, not a int32, hence the check 
			if(fill != 0) exit(6);
			memset(block, fill, hdr.blk_sz);
			for(unsigned i=0; i<chunk.chunk_sz; i++)
				if(write(1, block, hdr.blk_sz) != hdr.blk_sz) exit(8);
		} else if(chunk.chunk_type == CHUNK_TYPE_DONT_CARE) {
			if(chunk.total_sz != sizeof(chunk_header_t)) exit(9);

			memset(block, 0, hdr.blk_sz);
			for(unsigned i=0; i<chunk.chunk_sz; i++)
				if(write(1, block, hdr.blk_sz) != hdr.blk_sz) exit(10);
		} else if(chunk.chunk_type == CHUNK_TYPE_CRC32) {
			if(chunk.total_sz != 4 + sizeof(chunk_header_t)) exit(7);
			uint32_t crc32;
			if(read(0, &crc32, sizeof(crc32)) != sizeof(crc32)) exit(5);
			/* ignore crc32 */
		} else {
			exit(4);
		}
	}
	fsync(1);
	return 0;
}
