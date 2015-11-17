Server is a machine on operator side.
Client is a machine on our side. Tunnel is the client.

Create server and client machines
=================================
<pre>
$ vagrant up
</pre>

Setup everything
================
<pre>
$ cd ansible
</pre>
<pre>
$ ansible all -i hosts -m ping
server | success >> {
    "changed": false,
    "ping": "pong"
}

client | success >> {
    "changed": false,
    "ping": "pong"
}
</pre>
<pre>
$ ansible-playbook setup-all.yml -i hosts
</pre>
OR ONE BY ONE
<pre>
$ ansible-playbook setup-common.yml -i hosts
$ ansible-playbook setup-dsi.yml -i hosts
$ ansible-playbook setup-jss7.yml -i hosts
</pre>
<pre>
$ ansible-playbook setup-erlang.yml -i hosts --limit client
</pre>
<pre>
$ cd ..
</pre>

Test the DSI stack
==================
<pre>
$ vagrant ssh server
</pre>
[INSIDE]
<pre>
$ /opt/DSI/gctload -v
DSI gctload Release 6.7.1 (Build 1236)
Part of the Dialogic(R) DSI Development Package for Linux
Copyright (C) 1994-2015 Dialogic Corporation. All rights reserved.
Syntax: gctload [-c -d -v -t1[r] -t2 -t3 -t4 -x]
...
$ exit
</pre>
Start MTU & MTR example
=======================
See [MTU & MTR User Guide](https://www.dialogic.com/~/media/manuals/ss7/cd/GenericInfo/GeneralDocumentation/U30SSS03-MTU-MTR-UG.pdf)

Tab 1
-----
<pre>
$ vagrant ssh server
</pre>
[INSIDE]
<pre>
$ cd /opt/DSI/UPD/RUN/TUNNEL_SERVER/M3UA_CONFIG/
$ sudo ../../../../gctload -d
Reading from system configuration file: 'system.txt'
...
(14320)gctload: Verification started.
(14320)gctload: Verification complete.
...
S7_MGT Boot complete
</pre>

Tab 2
-----
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd /opt/DSI/UPD/RUN/TUNNEL_CLIENT/M3UA_CONFIG
$ sudo ../../../../gctload -d
Reading from system configuration file: 'system.txt'
...
(13729)gctload: Verification started.
(13729)gctload: Verification complete.
...
S7_MGT Boot complete

The server console now should have:
...
S7L:14:49:20.464 I0000 SCTP Assoc Status: assoc_id=0 i_streams=32 o_streams=32 CONNECTED
S7L:14:49:20.464 I0000 SCTP Path Status: assoc_id=0 (192.168.200.102) ACTIVE
...

The client console now should have:
...
S7L:14:49:20.259 I0000 SCTP Assoc Status: assoc_id=0 i_streams=32 o_streams=32 CONNECTED
S7L:14:49:20.259 I0000 SCTP Path Status: assoc_id=0 (192.168.200.101) ACTIVE
...
</pre>
We have connection established!

Tab 3
-----
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd /opt/DSI

$ ./mtu -d5 -a43020006 -g43010008 -e375291112233 -c375290000002
MTU MAP Test Utility  Copyright (C) Dialogic Corporation 1997-2011. All Rights Reserved.
===============================================================

MTU mod ID 0x2d; MAP module Id 0x15
mode 5 - Send routing info for Short Message (v3)

MTU Tx: sending Open Request
MTU Tx: I0000 M tc7e2 i0000 f2d d15 s00 p010b0906070400000100140301044302000603044301000800
MTU Tx: sending Send Routing Info for SMS Request
MTU Tx: I0000 M tc7e0 i0000 f2d d15 s00 p010e01010f07a17325191122331001011107a17325090000202d020f0000
MTU Tx: sending Delimiter Request
MTU Tx: I0000 M tc7e2 i0000 f2d d15 s00 p0500
MTU Rx: received Open Confirmation
MTU Rx: I0000 M t87e3 i0000 f15 d2d s00 p820501000b0906070400000100140301044301000803044302000600
MTU Rx: received Send Routing Info for SMS Confirmation
MTU Rx: I0000 M t87e1 i0000 f15 d2d s00 p820e0101120301020313079144214365870900
MTU Rx: received Close Indication
MTU Rx: I0000 M t87e3 i0000 f15 d2d s00 p0400
</pre>

Check out also:
<pre>
$ ./mtu -d5 -a43020006 -g43010008 -e375291112233 -c375290000002
$ ./mtu -d5 -a10001204732509000062 -g10001204732509000081 -e375291112233 -c375290000002
$ ./mtu -d0 -a43020008 -g43010008 -i987654321 -s"Hello"
$ ./mtu -d0 -a10001204732509000082 -g10001204732509000081 -i987654321 -s"Hello"
</pre>

Stop everything and exit.

Test the jSS7 simulator
=======================
See [Chapter 12. SS7 Simulator](ss7/docs/en-US/pdf/Mobicents_SS7Stack_User_Guide.pdf) inside [Mobicent jSS7 Simulator](ansible/files/mobicents-ss7-2.0.0.FINAL.zip)

Tab 1
-----
<pre>
$ vagrant ssh server
</pre>
[INSIDE]
<pre>
$ cd /opt/jSS7/ss7/mobicents-ss7-simulator
$ sudo bin/run.sh core --name=server --rmi=9999 --http=8888
</pre>

Tab 2
------
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd /opt/jSS7/ss7/mobicents-ss7-simulator
$ sudo bin/run.sh gui --name=server
</pre>

Control via RMI
---------------
<pre>
Select `Connect to the existing testerHost...'
service:jmx:rmi:///jndi/rmi://server:9999/server
Press `Start'
Press `Run test'
Press `Start'
</pre>

OR

Control via HTTP
----------------
<pre>
Go to http://192.168.200.101:8888/
SS7_Simulator_server -> type=TesterHost -> Press `start' button
</pre>

Tab 3
-----
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd /opt/jSS7/ss7/mobicents-ss7-simulator
$ sudo bin/run.sh gui --name=client
</pre>

Control via RMI
---------------
<pre>
Select `Create a local testerHost'
Press `Start'
Press `Run test'
Press `Start'
</pre>

We have connection established!

ENTER
<pre>
Message text: Hello
Destination ISDN number: 375296543210
Origination ISDN number: 375296543211
IMSI: 60802678000454
VLR number: 375290000001
</pre>
<pre>
Press `Send SRIforSM'
Press `Send SRIForSM and MtForwardSM'
Press `Send MtForwardSM'
</pre>

Stop everything and exit.

Build Tunnel
============
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
[Generate ssh key and add it to your github account](https://help.github.com/articles/generating-ssh-keys/)
<pre>
$ ssh-keygen -t rsa
$ cat ~/.ssh/id_rsa.pub
</pre>

Close and build Tunnel
----------------------
<pre>
$ cd ~/
$ git clone git@github.com:PowerMeMobile/tunnel_smpp.git
$ cd tunnel_smpp
$ make
$ exit
</pre>

Test Tunnel with the DSI stack (server is the MTR utility)
==========================================================

Tab 1 & 2
---------
See `Start MTU & MTR example' Tab 1 & 2

Tab 3
-----
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd ~/tunnel_smpp
$ make console
</pre>
[OUTSIDE]
<pre>
$ smppload -H192.168.200.102 -s375296543210 -d375296543211 -bhello
INFO:  Connected to 192.168.200.102:2775 (tcp)
INFO:  Bound to Tunnel
INFO:  Stats:
INFO:     Send success:     1
INFO:     Send fail:        0
INFO:     Delivery success: 0
INFO:     Delivery fail:    0
INFO:     Incomings:        0
INFO:     Errors:           0
INFO:     Avg Rps:          1 mps
INFO:  Unbound
</pre>

Stop everything and exit.

Test Tunnel with the jSS7 stack (server is jSS7 simulator)
==========================================================

Tab 1 & 2
---------
See `Test the jSS7 simulator' Tab 1 & 2

Tab 3
-----
See `Start MTU & MTR example' Tab 2

Tab 4
-----
<pre>
$ vagrant ssh client
</pre>
[INSIDE]
<pre>
$ cd ~/tunnel_smpp
$ make console
</pre>

[OUTSIDE]
<pre>
$ smppload -H192.168.200.102 -s375296543210 -d375296543211 -bhello
INFO:  Connected to 192.168.200.102:2775 (tcp)
INFO:  Bound to Tunnel
INFO:  Stats:
INFO:     Send success:     1
INFO:     Send fail:        0
INFO:     Delivery success: 0
INFO:     Delivery fail:    0
INFO:     Incomings:        0
INFO:     Errors:           0
INFO:     Avg Rps:          1 mps
INFO:  Unbound
</pre>

Stop everything and exit.
