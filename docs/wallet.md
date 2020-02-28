## Resync Windows wallet
In case you need to resync your local wallet in Windows:
* Close wallet;
* Create a shortcut;
* Rick click on shortcut and select properties;
* add in target, at the end of the path ` -reindex` (space -reindex, see screenshot below)
* wait full resync

![](assets/img/misc/win_wallet_reindex.png)

## Deleting masternodes cache
* Close the local wallet
* Navigate to your local SIN folder
	* Linux: `~/.sin/`
	* Mac: `~/Library/Application Support/SIN/`
	* Windows: %appdata%\SIN\
		* (This defaults to `C:\Documents and Settings\YourUserName\Application data\SIN\` on Windows XP and to `C:\Users\YourUserName\Appdata\Roaming\SIN\` on Windows Vista, 7, 8, and 10.)
* Remove the following files: `infinitynode.dat`, `mncache.dat`, `mnpayments.dat`
* Open your local wallet.
