#!/bin/bash

# stops nginx
sudo service nginx stop 

#stops uwsgi
killall -SIGINT uwsgi 2>/dev/null