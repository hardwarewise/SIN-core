SIN (X25X Algo) Mining Using T-Rex Miner

1-  Download T-Rex Latest Release

Download the current T-Rex version for your Nvidia graphics card.

Cuda 10 is preferred.

[https://github.com/trexminer/T-Rex/releases](https://github.com/trexminer/T-Rex/releases)

![](https://lh5.googleusercontent.com/v7HXXlfWw6KejY0aRUCL78jJhNmEYn_RjIc7xFsY_Ur9mr4ndD0QSSmWWb9uGBj9naUMzCGcG4nlTN9eKrwJjfwThqDTgtwGM9n2afsz3iJ2eXdunR68DbfzukhVEjXHOo8j_Q2k)

2-  Update Nvidia GPU Driver

If your video card driver is not up-to-date, it is recommended that you update it from the following address.

[https://www.nvidia.com/Download/index.aspx?lang=en-us](https://www.nvidia.com/Download/index.aspx?lang=en-us)

![](https://lh4.googleusercontent.com/bwnRpSXVt6WE0so8Wv0ki6d1LXgzrBqTBd9jj9HWl1CawF9xV9jtrlNVg3d5gmVJKLwKoAKe3sbE2kBXu3crr0JqmFQln0kZ9SbCBS2_TnMCKGv3kyxc9jFVCsk8NsY3w4C1YlNs)

  

3-  Creating a sample start.bat file

Go to the folder located in the T-Rex miner.

![](https://lh5.googleusercontent.com/KRT3jYQBdEL8QKR5xp7xubuX7EwYhKU3Siobu0SgCDJnNPxZh1U8zTQ0KEmnSpKcR12JE4fMwG1p4ytmqlYSF3ji82TzmKkN6C310Ds696hRLWTrRyVDT8TcpUiDemDjTffrYjgf)

  
  
  
  

Open a new Text Document

Server addresses by region are given below…

Europe stratum:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://eupool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

Asia stratum:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://asiapool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

USA stratum:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://uspool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

  

We used the European server in our example.

Please do not forget to write your own wallet address.

![](https://lh6.googleusercontent.com/A78gk_daWADOf9G-93EnBC3X9bQTnoHBtlXK9PBuiydSQjWipvjpkYn0LkRNEnyul3r8kr-TQOkO7TtULlT96OSnBRgbbSHo9UX7bFnrN4U5gLIwJhW1hwpnTZSVcKXD8STbZ_UY)

Select Save as and name the start.bat file.

![](https://lh5.googleusercontent.com/F_cs8nhbXVNrfFOBWsEtQeUaM2Px6RjLT6jtvQ2NYpt-6OyqGOBGS3x069JH0mK_ClD3icRQC0Bm5UPS3bKX0TKO5smuwSkq8A9qTBTyD4lU5BQIRNUXyZaspmn0FiblObxNQET_)

4-  Start the mining

Please double-click the start.bat file you just created. The T-Rex will connect to the server of your choice and start mining.

  

![](https://lh4.googleusercontent.com/mkFBxfk1vuHxD_8JvtSYGWPT70aZB2_LZ2__zVYN01k5p5M1T-ZEr4iPK6SPTwH76v4SRMA4VmDC9pSN4y5QBdF8OSQvSnYr1NpSe_LZrXSgxqCgYebcDTEPlEMw85wa_sM2_ocU)

For more detailed use of the T-Rex Miner, please see the Readme file at [https://github.com/trexminer/T-Rex](https://github.com/trexminer/T-Rex)
