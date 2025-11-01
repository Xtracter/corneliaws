
#include "../include/zlib.h"


int buf_compress(const char* input, int inputSize, char* output, int outputSize) {

    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = (uInt)inputSize;
    zs.next_in = (Bytef *)input;
    zs.avail_out = (uInt)outputSize;
    zs.next_out = (Bytef *)output;

    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    return zs.total_out;
}


