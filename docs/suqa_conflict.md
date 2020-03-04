## Guide For Old SUQA Coins Conflict

### 1. Go to SEND tab and open coin control

![img_01](assets/img/misc/suqa_conflict_01.png)

### 2. First, remove the conflicted transaction:
* [x] make a wallet backup.
* [x] close the wallet.
* [x] make a shortcut of your `sin-qt.exe` on the desktop. 
* [x] right click on it and select properties. 
* [x] Shortcut tab, in the Target field, add `-zapwallettxes` (see photo below)

![img_02](assets/img/misc/suqa_conflict_02.png)

### 3. Try sending using coin control:

![img_03](assets/img/misc/suqa_conflict_03.png)

* [x] select some inputs
* [x] send to yourself, but NOT on the same wallet.dat.  It can be an exchange wallet or running two SIN wallets on the same pc by changing the data directory and the port 
* [x] send all funds as "subtract fee from amount".