#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

# stops nginx
service nginx stop

#stops uwsgi
killall -SIGINT uwsgi 2>/dev/null
