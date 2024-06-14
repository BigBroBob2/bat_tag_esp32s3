#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef CB_H
#define CB_H
#endif

typedef struct {
    int N;
    short *buf; // actuall capacity = N-1
    int write_idx; // idx to write into circular buf
    int read_idx; // idx to read out from circular buf
} circular_buf;

bool is_empty(circular_buf *buf) {
    if (buf->read_idx == buf->write_idx) {
        return true;
    }
    else {
        return false;
    }
}

bool is_full(circular_buf *buf) {
    if ((buf->write_idx + 1) % buf->N == buf->read_idx) {
        return true;
    }
    else {
        return false;
    }
}

int buf_length(circular_buf *buf) {
    int temp = (buf->write_idx - buf->read_idx + buf->N) % buf->N;
    return temp;
}

int write_in_buf(circular_buf *buf, short *value, int L) {
    // L should be sizeof(value)/sizeof(short)
    if (L + buf_length(buf) > buf->N-1) {
        printf("circular_buf out of range, buf_length=%d \n", buf_length(buf));
        return -1;
    }
    
    for (int i = 0;i < L;i++) {
        buf->buf[buf->write_idx] = value[i];
        buf->write_idx = (buf->write_idx + 1) % buf->N;
    }
    return 0;
}

int read_out_buf(circular_buf *buf, short *value, int *buf_l) {
    int L = (int)buf_length(buf);
    buf_l[0] = L;
    for (int i=0;i<L;i++) {
        value[i] = buf->buf[buf->read_idx];
        buf->read_idx = (buf->read_idx + 1) % buf->N;
    }
    return 0;
}