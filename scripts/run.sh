set -e

_term() { 
  echo "Caught SIGTERM signal!" 
  pkill -TERM maind
  pkill -TERM pgbouncer
}

trap _term SIGTERM

pgbouncer pgbouncer.ini -d
./backend &
PID_BIN=$!
echo "I'm ready!"
wait $PID_BIN
