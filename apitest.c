#include <stdio.h>
#include "mosquitto.h"

void init (void);
void publish (void);
void subscribe (void);
void fini (void);

struct udata
{
  struct mosquitto *inst;
};

void init (void)
{
  int major, minor, revision;
  major = minor = revision = 0;
  mosquitto_lib_init();
  mosquitto_lib_version(&major, &minor, &revision);
  printf ("version = %d.%d.%d\n", major, minor, revision);
}

void fini (void)
{
  mosquitto_lib_cleanup();
}

int main (void)
{
  init();

  struct mosquitto *mqtt_inst;
  struct udata cdata;
  mqtt_inst = mosquitto_new ("test", true, &cdata);
  cdata.inst = mqtt_inst;

  fini();
  return (0);
}
