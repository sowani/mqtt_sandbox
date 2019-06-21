#ifndef MYSUB_H
#define MYSUB_H

#include "mosquittopp.h"

class MySub: public mosqpp::mosquittopp
{
    bool connected;

  public:
    MySub (const char* id, const char *host, int port);
    ~MySub();

    void on_connect (int rc);
    void on_disconnect (int rc);
    void on_subscribe (int mid, int qos_count, const int *granted_qos);
    void on_message (const struct mosquitto_message *message);
};
#endif // MYSUB_H
