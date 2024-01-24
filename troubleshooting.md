# Troubleshooting


## Step 1
### Step 1.5
- P:  The command `./wifi-install.sh wtsn_multi-q-patch-src.tar.gz –no-reboot` fails
    * S: Verify that `make` and `g++` are installed. To solve install `build-essential`. `sudo apt install build-essential`

## Step 2
### Step 2.1
- P:  There is no *screens* when issuing the command `sudo screen -ls`
    * S: Verify that the `NetworkManager` service is not running. If the command `systemctl status NetworkManager` says the service is *active* you should stop it with `sudo systemctl stop NetworkManager`. You can also disable it permanently with  `sudo systemctl disable NetworkManager`

## General issues
# Ping is too high
- If the ping time is too high (above 5-6ms) probably the Wifi NIC has enabled the power save. To disable it add the following options in a new line  after the `exit 0` in `/etc/modprobe.d/iwlwifi.conf` 

```
options iwlmvm power_scheme=1
options iwlwifi power_save=0
```

If you still have problem with high ping or spikes in latency during a test with Qbv, verify if the `sta0` interface is present. If so, remove it with `sudo iw dev sta0 del`
