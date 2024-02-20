set -e

_term() { 
  echo "Caught SIGTERM signal!" 
  pkill -TERM backend
  exit
}

trap _term SIGTERM

backend $PORT &
wait $!
