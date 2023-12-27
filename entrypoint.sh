#!/bin/bash
bitcoind -regtest -txindex=1 -port=19010 -rpcport=19011 -rpcbind=0.0.0.0 -rpcallowip=0.0.0.0/0 -conf=/.coordinate/chain.conf 
/bin/bash "$@"