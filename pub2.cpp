#include <iostream>
using namespace std;
#include <unistd.h>
#include "mypub.h"

int main (void)
{
  const char *topic = "counter";
  char c[2] = "b";
  class MyPub pub ("mypub2", "localhost", 1883);
  for (int i = 0; i < 10; i++)
  {
    cout << "publishing " << i << endl;
    pub.publish_msg (topic, c);
    sleep (4);
  }

  return (0);
}
