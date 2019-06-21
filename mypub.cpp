#include <iostream>
using namespace std;
#include "mypub.h"

MyPub::MyPub (const char* id, const char* host, int port) : mosquittopp(id)
{
  mosqpp::lib_init();

  this->keepalive = 60;
  this->id = id;
  this->host = host;
  this->port = port;
  connected = false; // Gets set in on_connected()

  connect (host, port, keepalive);
  loop_start();
}

MyPub::~MyPub()
{
  //loop_stop();
  //mosqpp::lib_cleanup();
}

void MyPub::on_connect (int rc)
{
  connected = true;
  cout << "MyPub::on_connect - " << connected << endl;
}

void MyPub::on_disconnect (int rc)
{
  connected = false;
  cout << "MyPub::on_disconnect - " << connected << endl;
  loop_stop();
  mosqpp::lib_cleanup();
}

void MyPub::on_publish (int mid)
{
  cout << mid << ": publish succeeded." << endl;
}

void MyPub::publish_msg (const char *topic, char *value)
{
    cout << "topic: " << topic << ", value: " << value << endl;
    // Publish the message over MQTT
    int rc = publish (NULL, topic, strlen(value), value);

    // Check for success
    if (rc == MOSQ_ERR_SUCCESS)
      cout << "PUBLISH successful: " << topic << " = " << value << endl;
    else
      cout << "PUBLISH failed: " << rc << endl;
}
