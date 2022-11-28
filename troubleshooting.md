# Troubleshooting


## Step 1
### Step 1.5
- P:  The command `./wifi-install.sh wtsn_multi-q-patch-src.tar.gz –no-reboot` fails
    * S: Verify that `make` and `g++` are installed. To solve install `build-essential`. `sudo apt install build-essential`

## Step 2
### Step 2.1
- P:  There is no *screens* when issuing the command `sudo screen -ls`
    * S: Verify that the `NetworkManager` service is not running. If the command `systemctl status NetworkManager` says the service is *active* you should stop it with `sudo systemctl stop NetworkManager`. You can also disable it permanently with  `sudo systemctl disable NetworkManager`
