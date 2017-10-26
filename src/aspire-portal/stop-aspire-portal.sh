#!/bin/bash

# stops nginx
service nginx stop

#stops uwsgi
killall -SIGINT uwsgi 2>/dev/null