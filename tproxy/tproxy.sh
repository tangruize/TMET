#!/bin/bash

function usage() {
  echo "Usage: sudo $0 -controller CONTAINER_NAME [-port TPROXY_PORT] [-subnet VIRTUAL_SUBNET] start/stop"
  exit 1
}

# check variables not null
function check() {
  while [ "$#" -ge 1 ]; do
    if [ -z "$1" ]; then
      usage
    fi
    shift
  done
}

if [ "$USER" != root ]; then
  echo "Error: Must run as root!"
  usage
  exit 1
fi

# for scripts to replace default value
CONTROLLER=${CONTROLLER}
PORT=${PORT}
SUBNET=${SUBNET}

# default value
PORT=${PORT:-1234}
SUBNET=${SUBNET:-10.1.1.0/24}

# arg parse
while [ "${1:0:1}" = "-" ]; do
  case "$1" in
  -p | -port | --tproxy-port)
    PORT="$2"
    shift 2
    ;;
  -s | -subnet | --virtual-subnet)
    SUBNET="$2"
    shift 2
    ;;
  -c | -controller | --controller-name)
    CONTROLLER="$2"
    shift 2
    ;;
  *)
    usage
    ;;
  esac
done

check "$PORT" "$SUBNET" "$CONTROLLER"

# exit on failure
set -eo pipefail

CONTROLLER_IP=$(lxc info "$CONTROLLER" | sed -En '/global/s/.*inet:[ ]*([0-9.]+).*/\1/p')
INTERFACE=$(ip route get "$CONTROLLER_IP" | head -1 | cut -d' ' -f3)

case "$1" in
start)
  set -x
  # add route controller to local (not forward)
  lxc exec "$CONTROLLER" -- ip route add local "${SUBNET}" dev lo
  # tproxy iptables rule in controller node
  lxc exec "$CONTROLLER" -- iptables -t mangle -A PREROUTING -d "${SUBNET}" -p tcp -m tcp -j TPROXY --on-port "${PORT}" --on-ip 127.0.0.1
  # add route host to controller
  ip route add "$SUBNET" via "$CONTROLLER_IP" dev "${INTERFACE}"
  # enable ip forwarding
  sysctl -w net.ipv4.ip_forward=1 > /dev/null
  # no network address translation (NAT) for this subnet
  iptables -t mangle -I POSTROUTING -d "${SUBNET}" -j ACCEPT
  ;;
stop)
  set -x
  ip route del "${SUBNET}" via "${CONTROLLER_IP}" dev "${INTERFACE}"
  iptables -t mangle -D POSTROUTING -d "${SUBNET}" -j ACCEPT
  lxc exec "$CONTROLLER" -- ip route del local "${SUBNET}" dev lo
  lxc exec "$CONTROLLER" -- iptables -t mangle -D PREROUTING -d "${SUBNET}" -p tcp -m tcp -j TPROXY --on-port "${PORT}" --on-ip 127.0.0.1
  ;;
*)
  usage
  ;;
esac
