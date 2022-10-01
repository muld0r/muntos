#ifndef RT_LOG_H
#define RT_LOG_H

void rt_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* RT_LOG_H */
