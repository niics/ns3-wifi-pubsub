ifconfig br-sub1 down
ifconfig br-mid down
ifconfig br-pub1 down

brctl delif br-sub1 tap-sub1
brctl delif br-mid tap-mid
brctl delif br-pub1 tap-pub1

brctl delbr br-sub1
brctl delbr br-mid
brctl delbr br-pub1

ifconfig tap-sub1 down
ifconfig tap-mid down
ifconfig tap-pub1 down

tunctl -d tap-sub1
tunctl -d tap-mid
tunctl -d tap-pub1

brctl show
