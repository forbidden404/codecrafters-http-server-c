#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "compression.h"

int gzip_compress(const char *input, size_t input_len, unsigned char **output,
                  size_t *output_len) {
  z_stream zs = {0};

  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                   15 + 16, /* gzip header */
                   8, Z_DEFAULT_STRATEGY) != Z_OK) {
    return -1;
  }

  zs.next_in = (Bytef *)input;
  zs.avail_in = input_len;

  int ret;

  size_t capacity = 1024;
  *output = malloc(capacity);

  zs.next_out = *output;
  zs.avail_out = capacity;

  while ((ret = deflate(&zs, Z_FINISH)) == Z_OK) {
    size_t used = zs.total_out;

    capacity *= 2;

    *output = realloc(*output, capacity);

    zs.next_out = *output + used;
    zs.avail_out = capacity - used;
  }

  if (ret != Z_STREAM_END) {
    free(*output);
    deflateEnd(&zs);
    return -1;
  }

  *output_len = zs.total_out;

  deflateEnd(&zs);

  return 0;
}
