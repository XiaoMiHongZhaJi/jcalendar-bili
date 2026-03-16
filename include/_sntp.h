#ifndef ___SNTP_H__
#define ___SNTP_H__

int sntp_status();
void sntp_exec(int status = 0);

typedef enum {
    SYNC_STATUS_IDLE = -1,
    SYNC_STATUS_IN_PROGRESS = 0,
    SYNC_STATUS_OK = 1,
    SYNC_STATUS_NOK = 2,
    SYNC_STATUS_TOO_LATE = 3
} sntp_status_t;

#endif