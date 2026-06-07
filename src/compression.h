#ifndef _COMPRESSION_H
#define _COMPRESSION_H

#include <stddef.h>

int gzip_compress(const char *input, size_t input_len, unsigned char **output,
                  size_t *output_len);

#endif
