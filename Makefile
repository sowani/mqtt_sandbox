INC_PATH = -I$(HOME)/MQTT/usr/local/include
LIB_PATH = -L$(HOME)/MQTT/usr/local/lib
LIB = -lmosquitto
MQTT_FLAGS = -DWITH_TLS -DWITH_TLS_PSK -DWITH_THREADING -DWITH_SOCKS

target = apitest

%.o: %.c
	gcc $(MQTT_FLAGS) -c $< -o $@ $(INC_PATH)

apitest: apitest.o
	gcc $< -o $@ $(INC_PATH) $(LIB_PATH) $(LIB)

clean:
	\rm -f *.o apitest
