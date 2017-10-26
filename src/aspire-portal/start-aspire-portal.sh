#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

cd $(dirname $0)

# ensure that nginx is running
service nginx start

# ensure that no other instances are running
killall -SIGINT uwsgi 2>/dev/null || true

# start the ASPIRE Portal
/usr/local/bin/uwsgi aspire-portal.ini 2>>/opt/online_backends/aspire_service.log &
