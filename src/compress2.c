#include <stdio.h>
#include <string.h>
#include "../include/zlib.h"

#define CHUNK 16384  // 16 KB chunks



// Compress input data to output using deflate()
int compress_stream(const unsigned char *input, size_t input_len,
                    unsigned char *output, size_t *output_len) {
    z_stream strm;
    unsigned char out[CHUNK];
    size_t total_out = 0;
    int ret;

    // Initialize the zlib stream
    memset(&strm, 0, sizeof(strm));
    if (deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK)
        return Z_ERRNO;

    strm.next_in = (Bytef *)input;
    strm.avail_in = input_len;

    // Process until input is fully consumed
    do {
        strm.next_out = out;
        strm.avail_out = CHUNK;

        ret = deflate(&strm, strm.avail_in ? Z_NO_FLUSH : Z_FINISH);

        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            return Z_ERRNO;
        }

        size_t have = CHUNK - strm.avail_out;
        memcpy(output + total_out, out, have);
        total_out += have;
    } while (ret != Z_STREAM_END);

    deflateEnd(&strm);
    *output_len = total_out;
    return Z_OK;
}

// Decompress input data to output using inflate()
int decompress_stream(const unsigned char *input, size_t input_len,
                      unsigned char *output, size_t *output_len) {
    z_stream strm;
    unsigned char out[CHUNK];
    size_t total_out = 0;
    int ret;

    memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK)
        return Z_ERRNO;

    strm.next_in = (Bytef *)input;
    strm.avail_in = input_len;

    do {
        strm.next_out = out;
        strm.avail_out = CHUNK;

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm);
            return ret;
        }

        size_t have = CHUNK - strm.avail_out;
        memcpy(output + total_out, out, have);
        total_out += have;
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    *output_len = total_out;
    return Z_OK;
}

/*
int main(void) {

//    const char* text = "This is a longer example string repeated a few times. "
  //                     "This is a longer example string repeated a few times. "
    //                   "This is a longer example string repeated a few times. ";

    int size=2048;
    char text[size];

    FILE* fd = fopen("/home/nrkfrr/GIT/corneliaws/www/corn2.png","rb");
    if(fd==NULL){
      printf("Bad file\n");
      return -1;
    }
    //size_t input_len = strlen(text) + 1;
    size_t input_len = size;
    unsigned char compressed[size];
    unsigned char decompressed[size];
    size_t compressed_len = 0;
    size_t decompressed_len = 0;
    int r = 0;

    FILE* fd2 = fopen("corn2.png","wb");
    while((r=fread(text,1,size,fd))>0){

      if (compress_stream((const unsigned char *)text, input_len,
                          compressed, &compressed_len) != Z_OK) {
          fprintf(stderr, "Compression failed.\n");
          return 1;
      }

      printf("Original size: %zu\n", input_len);
      printf("Compressed size: %zu\n", compressed_len);

      if (decompress_stream(compressed, compressed_len,
                            decompressed, &decompressed_len) != Z_OK) {
          fprintf(stderr, "Decompression failed.\n");
          return 1;
      }

      printf("Decompressed size: %zu\n", decompressed_len);
      fwrite(decompressed,1,decompressed_len,fd2);
    }
    fclose(fd2);

    return 0;
}
*/

