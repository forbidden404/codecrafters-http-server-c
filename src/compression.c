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

  size_t buffer_size = compressBound(input_len);
  *output = malloc(buffer_size);

  zs.next_in = (Bytef *)input;
  zs.avail_in = input_len;

  zs.next_out = *output;
  zs.avail_out = buffer_size;

  int ret = deflate(&zs, Z_FINISH);

  if (ret != Z_STREAM_END) {
    free(*output);
    deflateEnd(&zs);
    return -1;
  }

  *output_len = zs.total_out;

  deflateEnd(&zs);

  return 0;
}
