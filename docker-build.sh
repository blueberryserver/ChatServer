#!/bin/bash
docker build -t chat-server -f docker/Dockerfile .

docker run --rm -it -p 12345:12345 chat-server