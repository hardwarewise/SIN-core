
# Windows QT Wallet Backup and Upgrade Guide

* First of all, don't forget to back up your current data.

* Close your wallet.

* Open the Run window using the WIN + R key combination.


* Enter the `%appdata%/SIN` command and click the OK button. Thus, you will reach the directory where your SIN data is kept.

![run](assets/img/misc/run.png)
 

Your wallet.dat file can be either in the wallets directory or in the SIN home directory, depending on the situation. Pay attention to this and back up these files to a suitable medium. We recommend that you do this periodically.

  
![directory](assets/img/misc/directory.png)


Download the latest version of Windows Wallet at [https://github.com/SINOVATEblockchain/SIN-core/releases](https://github.com/SINOVATEblockchain/SIN-core/releases)
 

![release](assets/img/misc/release.png)
  

Replace the `sin-qt.exe` file in the compressed file you downloaded with the `sin-qt.exe` file you are currently using.


**Resync Windows wallet**
In case you need to resync your local wallet in Windows:
* Close wallet;
* Create a shortcut;
* Rick click on shortcut and select properties;
* add in target, at the end of the path ` -reindex` (space -reindex, see screenshot below)
* wait full resync

![](assets/img/misc/win_wallet_reindex.png)


# MAC QT Wallet Backup and Upgrade Guide

  

* First of all, don't forget to back up your current data.

* Close your wallet.

* In order to see hidden folders, such as `~/Library` from Finder, simply hit `shift + ⌘ (command) + G` which will GO to a folder, then paste in this location:


* ```~/Library/Application Support/SIN```

![run](assets/img/misc/mac_backup01.png)

![run2](assets/img/misc/mac_backup02.png)

![run3](assets/img/misc/mac_backup03.png)
 

Your wallet.dat file can be either in the wallets directory or in the SIN home directory, depending on the situation. Pay attention to this and back up these files to a suitable medium. We recommend that you do this periodically.

  
![run4](assets/img/misc/mac_backup04.png)

![run5](assets/img/misc/mac_backup05.png)


Download the latest version of MAC Wallet at [https://github.com/SINOVATEblockchain/SIN-core/releases](https://github.com/SINOVATEblockchain/SIN-core/releases)
 

![run6](assets/img/misc/mac_backup06.png)
  

Replace the `sin-qt app` file in the compressed file you downloaded with the `sin-qt app` file you are currently using.


# SINOVATE InfinityNode Update And Sync Guide

**If running Infinity Node, stop it.**
``sudo systemctl stop sinovate.service``

**install unzip package**
``sudo apt update && sudo apt install unzip``

**remove old files and folders (Write this entire code on one line.)**
``rm -rf ~/.sin/{blocks,chainstate,debug.log,mnpayments.dat,mncache.dat,banlist.dat,peers.dat,netfulfilled.dat,governance.dat,fee_estimates.dat}``

**download latest bootstrap archive**
``wget -O ~/bootstrap.zip https://github.com/SINOVATEblockchain/SIN-core/releases/latest/download/bootstrap.zip``

**unzip the bootstrap archive**
``unzip ~/bootstrap.zip``

**move bootstrap files**
``mv -t ~/.sin ~/bootstrap/blocks ~/bootstrap/chainstate``

**remove unnecessary files**
``rm -rf ~/{bootstrap,bootstrap.zip}``

**Update Latest Wallet**
``wget -O daemon.tar.gz https://github.com/SINOVATEblockchain/SIN-core/releases/latest/download/daemon.tar.gz``
``tar -xzvf daemon.tar.gz``
**Check Version**
``./sin-cli -version``
**reboot infinitynode**
``sudo reboot``


