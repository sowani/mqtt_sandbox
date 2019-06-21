#include <iostream>
using namespace std;
#include <unistd.h>
#include "mypub.h"

int main (void)
{
  const char *topic = "counter";
  char c[2] = "a";
  class MyPub pub ("mypub", "localhost", 1883);
  for (int i = 0; i < 10; i++)
  {
    cout << "publishing " << i << endl;
    pub.publish_msg (topic, c);
    sleep (2);
  }

  return (0);
}
