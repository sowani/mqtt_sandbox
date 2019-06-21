#ifndef MYPUB_H
#define MYPUB_H

#include <cstring>
#include "mosquittopp.h"

class MyPub: public mosqpp::mosquittopp
{
    int keepalive;
    const char *id;
    const char *host;
    int port;
    bool connected;

  public:
    MyPub (const char* id, const char *host, int port);
    ~MyPub();

    void publish_msg (const char *topic, char *value);

    void on_connect (int rc);
    void on_disconnect (int rc);
    void on_publish (int mid);
};
#endif // MYPUB_H
