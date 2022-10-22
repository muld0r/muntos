#ifndef RT_LOG_H
#define RT_LOG_H

void rt_logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* RT_LOG_H */
