set -e

_term() { 
  echo "Caught SIGTERM signal!" 
  pkill -TERM maind
  pkill -TERM pgbouncer
}

trap _term SIGTERM

pgbouncer pgbouncer.ini -d
sleep 5
echo "pgbouncer ready!"
./backend &
PID_BIN=$!
sleep 5
echo "I'm ready!"
wait $PID_BIN
