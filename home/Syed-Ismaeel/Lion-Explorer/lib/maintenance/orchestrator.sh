#!/bin/bash
TARGETS_FILE="targets.txt"
echo "🦁 Lion Explorer: Autonomous Engine Online"
while true; do
  if [ -f "$TARGETS_FILE" ]; then
    echo "🦁 [AI Agent Thinking]: Analyzing targets..."
    for TARGET in $(cat $TARGETS_FILE); do
       echo "🦁 [AI Agent Act]: Investigating surface of $TARGET..."
       ~/Lion-Explorer/lib/deep-fusion/scanner-advanced.sh $TARGET
    done
  else
    echo "🦁 [AI Agent Idle]: No targets found. Waiting..."
  fi
  sleep 3600
done
