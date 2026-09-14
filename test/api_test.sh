#! /bin/bash

HOST=$1
if [ -z "$HOST" ]; then
    echo "Error: The host to connect to is missing!"
    echo "usage: api_test.sh hostname"
    exit 1
fi

echo -e "\r\nIncrease volume a few times\n\n"
curl http://$HOST:9000/api/vup
sleep 1
curl http://$HOST:9000/api/vup
sleep 1
curl http://$HOST:9000/api/vup
sleep 1

echo -e "\r\n\r\nDecrease volume a few times\r\n"
curl http://$HOST:9000/api/vdown
sleep 1
curl http://$HOST:9000/api/vdown
sleep 1
curl http://$HOST:9000/api/vdown
sleep 1

echo -e "\r\n\r\nIncrease tone a few times\r\n"
curl http://$HOST:9000/api/tup
sleep 1
curl http://$HOST:9000/api/tup
sleep 1
curl http://$HOST:9000/api/tup
sleep 1

echo -e "\r\n\r\nDecrease tone a few times\r\n"
curl http://$HOST:9000/api/tdown
sleep 1
curl http://$HOST:9000/api/tdown
sleep 1
curl http://$HOST:9000/api/tdown
sleep 1
echo -e "\r\n"
