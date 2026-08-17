#ifndef FILE_ANALYZE_H
#define FILE_ANALYZE_H

#define SHA256_DIGEST_SIZE 32

struct file_info {
    unsigned long size;
    unsigned long timestamp;
    unsigned char hash[SHA256_DIGEST_SIZE];
};

#endif
