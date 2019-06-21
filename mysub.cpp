#include <cstring>
#include <iostream>
using namespace std;
#include "mysub.h"

MySub::MySub (const char *id, const char *host, int port) : mosquittopp(id)
{
  mosqpp::lib_init();

  int keepalive = 60;
  connected = false; // Gets set in on_connected()

  connect (host, port, keepalive);
  loop_start();
}

MySub::~MySub()
{
  loop_stop();
  mosqpp::lib_cleanup();
}

void MySub::on_connect (int rc)
{
  if (rc == 0)
    connected = true;
  else
    connected = false;
  subscribe (NULL, "counter");
}

void MySub::on_disconnect (int rc)
{
  connected = false;
}

void MySub::on_subscribe (int mid, int qos_count, const int *granted_qos)
{
  cout << mid << ": subscription succeeded." << endl;
}

void MySub::on_message (const struct mosquitto_message *message)
{
    if (!connected)
        return;

    char value[50];
    memset (value, 0, 50*sizeof(char));
    memcpy (value, message->payload, 50*sizeof(char));
    cout << "Received: " << message->topic << " = " << value << endl;
}
