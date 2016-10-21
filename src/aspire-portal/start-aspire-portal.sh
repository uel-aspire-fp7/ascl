#!/bin/bash

cd $(dirname $0)

# ensure that nginx is running
sudo service nginx start 

# ensure that no other instances are running
killall -SIGINT uwsgi 2>/dev/null

# start the ASPIRE Portal
/usr/local/bin/uwsgi aspire-portal.ini 2>>/opt/ASCL/aspire-portal/aspire_service.log &
