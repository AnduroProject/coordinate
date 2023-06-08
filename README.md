# Mara Node

## How to Setup Mara Node

1. Clone Mara Node 
   <pre>https://github.com/MarathonDH/mara-sidechain-node.git</pre>

2. Install sidechain dependencies
   <pre>sudo apt-get install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3</pre>
   <pre>sudo apt-get install libevent-dev libboost-dev</pre>
   <pre>sudo apt install libsqlite3-dev</pre>
   <pre>sudo apt-get install libzmq3-dev</pre>
   <pre>sudo apt install systemtap-sdt-dev</pre>

3. Install GUI dependencies
   <pre>sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools</pre>
   <pre>sudo apt install qtwayland5</pre>
   <pre>sudo apt-get install libqrencode-dev</pre>

4. Go to the file <b>src/chainparams.cpp</b>
   <b>line number : 40 replace corresponding 0th block address based on the Network (regtest or testnet or mainnet)</b>
   <pre>genesis.nextAddress = replace corresponding 0th block address based on the Network  (regtest or testnet or mainnet)</pre>

5. Go to the file<b>src/federation_deposit.cpp</b>
   <b>line number : 20 replace corresponding 0th block address based on the Network (regtest or testnet or mainnet)</b>
   <pre>GENSIS_NEXTADDRESS = replace corresponding 0th block address based on the Network  (regtest or testnet or mainnet)</pre>

6. To generate the configuration file, run the following commands in the root path.
   <pre>mkdir ~/.marachain/</pre>
   <pre>touch ~/.marachain/marachain.conf</pre>

7. go to the <b>.marachain</b> folder and open the <b>marachain.conf</b> file paste the below line.
   <pre>
    rpcuser=marachain
	rpcpassword=mara
	server=1
	zmqpubrawblock=tcp://127.0.0.1:27000
	zmqpubrawtx=tcp://127.0.0.1:27000
	zmqpubhashtx=tcp://127.0.0.1:27000
	zmqpubhashblock=tcp://127.0.0.1:27000
	rest=1</pre>

8. go to the sidechain root directory run the following commands
   <pre>./autogen.sh</pre>
   <pre>./configure</pre>
   <pre>make</pre>
   <pre>cd src</pre>

9. start the sidechain node run the below comment</b>
   <b>for regtest</b>
   <pre>./bitcoind --regtest --conf=/configuration_file_path/marachain.conf --datadir=/configuration_file_path/.marachain -fallbackfee=0.0001 -txindex=1</pre>
   <b>for testnet</b>
   <pre>./bitcoind --testnet --conf=/configuration_file_path/marachain.conf --datadir=/configuration_file_path/.marachain -fallbackfee=0.0001 -txindex=1</pre>
   <b>for mainnet</b>
   <pre>./bitcoind --conf=/configuration_file_path/marachain.conf --datadir=/configuration_file_path/.marachain -fallbackfee=0.0001 -txindex=1</pre>    