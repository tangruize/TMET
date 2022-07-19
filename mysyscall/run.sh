#!/bin/bash

function usage() {
  echo "$0 [-library LIBRARY_FILE] [-config CONFIG_FILE] COMMAND [ARGS]"
  exit 1
}

function check() {
  if [ -n "$1" -a ! -r "$1" ]; then
    echo "Error: cannot read file \"$1\""
    exit 1
  fi
}

if [ -n "${MYSYSCALL_CONFIG}" ]; then
  export "MYSYSCALL_CONFIG=${MYSYSCALL_CONFIG}"
elif [ -n "${CONFIG}" ]; then
  export "MYSYSCALL_CONFIG=${CONFIG}"
fi

if [ -n "${LIBRARY}" ]; then
  LIBRARY="${LIBRARY}"
fi

while [ "${1:0:1}" = "-" ]; do
  case "$1" in
  -c|-config|--config)
    export "MYSYSCALL_CONFIG=$2"
    shift 2
    ;;
  -l|-library|--library)
    LIBRARY="$2"
    shift 2
    ;;
  *)
    usage
    ;;
  esac
done

if [ -z "$LIBRARY" ] || [ -z "$1" ]; then
  usage
fi

check "$LIBRARY"
check "$MYSYSCALL_CONFIG"

env "LD_PRELOAD=$LIBRARY" "$@"
