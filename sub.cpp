#include "mysub.h"

int main (void)
{
  class MySub sub("mysub", "localhost", 1883);
  while (1)
  {
    int rc;
    rc = sub.loop();
    if (rc)
      sub.reconnect();
  }

  return (0);
}
