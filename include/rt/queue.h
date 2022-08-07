#pragma once

#include <rt/rt.h>

#include <stdbool.h>
#include <stddef.h>

struct rt_queue_config
{
    void *buf;
    size_t elem_size;
    size_t num_elems;
};

struct rt_queue;

void rt_queue_init(struct rt_queue *queue,
                    const struct rt_queue_config *cfg);

void rt_queue_send(struct rt_queue *queue, const void *elem);

void rt_queue_recv(struct rt_queue *queue, void *elem);

#define RT_QUEUE_FROM_ARRAY(name, array)                                      \
    struct rt_queue name = {                                                  \
        .recv_list = RT_LIST_INIT(name.recv_list),                            \
        .send_list = RT_LIST_INIT(name.send_list),                            \
        .buf = array,                                                          \
        .len = 0,                                                              \
        .read_offset = 0,                                                      \
        .write_offset = 0,                                                     \
        .capacity = sizeof(array),                                             \
        .elem_size = sizeof((array)[0]),                                       \
    }

struct rt_queue
{
    struct rt_list recv_list;
    struct rt_list send_list;
    void *buf;
    size_t len;
    size_t read_offset;
    size_t write_offset;
    size_t capacity;
    size_t elem_size;
};
