#ifndef ROACH_INTERFACE_H
#define ROACH_INTERFACE_H

#include <stdint.h>
#include <string>
#include <pthread.h>

#include <list>
#include <string>

struct katcp_dispatch;

class RoachInterface
{
public:
    RoachInterface(const char *hostname, uint16_t port, bool simulate = false);
    ~RoachInterface();

    void newAddress(const char *hostname, uint16_t port);
    bool arm();
    bool loadBof(const char *bofname);
    bool unloadBof();
    bool getValue(const char *reg, int8_t *buffer, int32_t length,
                  uint32_t offset = 0);
    bool getValue(const char *reg, uint32_t &value, uint32_t offset = 0);
    bool setValue(const char *reg, const int8_t *buffer, int32_t length,
                  uint32_t offset = 0);
    bool setValue(const char *reg, uint32_t value, uint32_t offset = 0);
    bool tapStart(const char *device, const char *reg, const char *ip,
                  int32_t port = -1, const char *mac = "");
    bool tapStop(const char *reg);

    // I2C commands
    bool getI2cValue(uint8_t addr, uint32_t nbytes, uint8_t *data);
    bool setI2cValue(uint8_t addr, uint32_t nbytes, const uint8_t *data);
    void setTestMode(bool test_mode);
    bool status();

    // test func
    //bool intdiv(int a, int b, int &c);


private:

    bool resultOk(std::string cmd, uint32_t stimeout = 0,
                  uint32_t ustimeout = 500000);

    int32_t fd;
    katcp_dispatch *kd;
    char *host;
    int16_t port;
    bool sim;

    pthread_mutex_t lock;
};

#endif//ROACH_INTERFACE_H
