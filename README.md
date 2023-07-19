# Mara Federation Management

## How to Setup Mara Sidechain node

1. Clone Mara Sidechain Node 
   <pre>https://github.com/MarathonDH/mara-sidechain-node.git</pre>
2. Go to home directory & create directory
   <pre> mkdir -p /home/.marachain</pre>
3. Create a configuration file under configuration directory.
   <pre> touch /home/.marachain/marachain.conf</pre>
4. Update the config details as mentioned in the below example.
5. To build & run the node. Run following commands from terminal with the dependencies needed.
   <pre>./autogen.sh</pre>
   <pre>./configure</pre>
   <pre>make</pre>
   <pre>make install</pre>
6. Make sure to enable the ports needed for the node based on the network in hosting firewall or security groups

## marachain.conf example

 rpcuser=marachain
 rpcpassword=mara
 server=1
 # zmq settings
 zmqpubrawblock=tcp://0.0.0.0:37000
 zmqpubrawtx=tcp://0.0.0.0:37000
 zmqpubhashtx=tcp://0.0.0.0:37000
 zmqpubhashblock=tcp://0.0.0.0:37000
 # Allow rpc settings
 rest=1
 rpcbind=0.0.0.0
 rpcallowip=0.0.0.0/0
 rpcport=19011

# Docker for Mara Sidechain node

## System Requirement

   <pre>OS: Ubuntu 20.04 LTS</pre> 

## Install Docker

   # Add Docker’s official GPG key:
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

   #  set up the stable repository x86
  echo \
      "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
      $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

## Todo the config settings on the Core package

1. Clone Mara Federation Management 
   <pre>https://github.com/MarathonDH/mara-sidechain-node.git</pre>
2. Go to home directory & create directory
   <pre> mkdir -p /home/.marachain</pre>
3. Create a configuration file under configuration directory.
   <pre> touch /home/.marachain/marachain.conf</pre>
4. Update the config details.
5. Goto root directory & change the config path in the file <pre>entrypoint.sh</pre>
4. Run docker build
   <pre>docker build -t marasidechain .</pre>
4. Run docker image with desired port 
   <pre>docker run -d --name marasidechain marasidechain</pre>
