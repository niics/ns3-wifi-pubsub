#!/bin/sh
brctl addbr br-sub1
brctl addbr br-mid
brctl addbr br-pub1
tunctl -t tap-sub1
tunctl -t tap-mid
tunctl -t tap-pub1
ifconfig tap-sub1 0.0.0.0 promisc up
ifconfig tap-mid 0.0.0.0 promisc up
ifconfig tap-pub1 0.0.0.0 promisc up

brctl addif br-sub1 tap-sub1
brctl addif br-mid tap-mid
brctl addif br-pub1 tap-pub1

ifconfig br-sub1 up
ifconfig br-mid up
ifconfig br-pub1 up

brctl show

