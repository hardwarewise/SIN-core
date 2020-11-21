# SINOVATE INFINITYNODE AND D.I.N. DOUBLE-RUN GUIDE


This guide assumes you already own an infinity node running.

**Please before applying the manual,**

1- Update your node.

2- Resynchronize.

To do this, follow the instructions on the link below.

VPS Infinity Node Update And Sync Guide [here](https://docs.sinovate.io/#/wallet_upgrade?id=vps-infinity-node-update-and-sync-guide).

**If you've already followed these instructions, you can continue here.**

Connect to your VPS and enter the commands below.

```bash
bash
/sin-cli infinitynode keypair
```

![](https://lh6.googleusercontent.com/-gsD8Y15gVfUwOPG8uTFkHz_RGiHf_o9gPF2tesAz2Ivq4bzTRTAJkCq9eV9uZLU1s8Y0DIA4zBJWkaKnqTab7729A7C7Flbe8flBKeNr7Hpg-al3U1qC5GP6UBGJYlTZfFTJnFH)

  

Please save the keypair result

.![](https://lh3.googleusercontent.com/4q2rYkqFKR9fq3neYoRY8C-zNh8sSTZHhGvQVhBY5jzepVy5xe1Dxmi7JvCNkTsKA-gnt0MzP7t_Jo718SO5qpR6bS8v-j8WbK8bb1BHZM4EuABz5THM3Z1rTQndVM1sNP4JQLcx)  
  

(PuTTy will automatically copy the area you selected with the mouse.)

  

![](https://lh6.googleusercontent.com/rNtw9CDllqMZeIDB2KZ57qp3b4Cl-Dgs6E9pdPS9X0YcmH2q7Ig8agkQ0yCkJeJhsAEfGsvRv7C1YiRECQ0d_JAabGfCA1wjI06TmSYttTLBBAJpElO_hP9eK__RvN_P5drSbKOP)

  

Paste the copied result into a Notepad-like text editor. You will need this information in the next stages.

  

![](https://lh6.googleusercontent.com/tJOjqPFb_4ANA1FSc6R_3rLx9I0z_BCmVvYiXo-QM99SB8M3SlfCWs-WlDxo8N3mvnIaUG_K1ETbUQHKvm-3WDqYykOUy3JM2pF2zSRkF1-Vty0P6UXlpC_6jS6WJjINDbHDojp8)

  
  
  

Copy the PrivateKey given to you.

  

![](https://lh5.googleusercontent.com/czFyJUIIYUPdyTtTKqRpdzp4pFMhR-qrgNiKZPpwCOJKWMq8fW88MWZhKDamV-LyNyaQ38rfWPFIuSxMGmpLkmwg1dzSllrP_Wwr_Y9OEssIvrTEUXy5VoLAcK8bk20njCFtMGfx)

  

Enter the following command on the VPS screen and paste the key you copied, leaving a space.  
(PuTTY will paste the copied data with a right mouse click.)

  

```bash
./sin-cli importprivkey [your_privatekey]  
```  

(Please use your own key and address. The data here is for informational purposes only.)

  

![](https://lh6.googleusercontent.com/Xa0G-lpvC6YT6SZJmwh0-nQ8pwsfhbkWYJmHC2B29tKCyfvjz3O8mXDynbAVKaUoLY_qaGZ5NfrukCaLQtgIcZ9JKFhOOYoTMyWysKcRWJVZGnOdC85dpnmZg2eO_GIJzhK5jIO4)  
  
  

  
  
  

Enter the following command to open the sin.conf file.  
  
```bash
sudo nano .sin/sin.conf  
```
  
![](https://lh5.googleusercontent.com/d8TYEJHa9GFtMErfsGNYd9IR72O_k2QZ5CDQ6rof25FF8ARn0QA3xEJiM6ftohxJS5_hWsRppys5fUtp0a_YjbOCeG0EX5Zw1f2DRxrsdygHGlSxDbcc8JhxktX1RSaHTafLbBa_)

  

Please enter the new information marked below. Remember we previously saved the infinity node privkey info in a Notepad-like editor.

  

infinitynode=1

infinitynodeprivkey=[your_infinitynodeprivkey]  
  

![](assets/img/misc/double-run-01.png)

  

If you are sure that you entered the information correctly, close the sin.conf file with the key combinations Ctrl + O ENTER and Ctrl + X ENTER.

  

Restart your VPS with the

  
```bash
sudo reboot
```


![](https://lh3.googleusercontent.com/3TMlZcHvvjjQ-a6CNoo1iTWMn3MAHQotuEM4OR6qBKTY43kAEv6B4uQpfHlXqL7-APumtUhvf_DSQLyvGXIX70z2AexSDKsk2ckDWKGzFqtzud8Q4z0cMmVLx1d9Kh34T7BJO9PZ)  
command.

  

Open your control wallet and send 1 SIN to your vps wallet address.

  
  

![](https://lh5.googleusercontent.com/FrEhr6n9CYZYscYesy274x6eyDPHLlrXgvH_kN2Gmy6khXlwPaoow6uVJXRxHSTgTSEFSxcwtlqT99aUeki7lhqi6sCXu3tOGjN0zjNp0w92NuHhImAPxN1FtqmTdhNGwS3OjYnS)

  
  

Then send 25 SINs to the owner address of your working masternode.

  

Please wait for 2 confirmations!

  

Open the Debug Console and enter the following command.

  
  

infinitynodeupdatemeta [Owner address] [node PublicKey] [node IP:20970] [first 16 characters of BurnFundTx]

  

![](https://lh5.googleusercontent.com/HyypFZAMPEXMTKSBxAm6YFAurPAv0a4INr7k5PjC3Aep69V3S0CyH_nVQRKCUSdRNYCjG0VJKq9FW1doTO6hj28zAjXDCkIo1ItsvrbhvQGFOU6UI_dKPT606nkpayZ1SePkeT7s)

  
  
  
  
  
  
  

You can get the first 16 characters of BurnTx from the infinitynode.conf file.

  

![](https://lh4.googleusercontent.com/bquq-c_aAd2NV1B0mmfbFil8VLnx1_0_sKGchuelIYRTwu7TwfMgzK_9VV7Xv-YXIE2NWAvyl_nZ2aslqITAQ6_4QMwbtm8imy2czG4HDss-a05tGanpA8XnZHsq3mO6_SyFHrsi)

  
  

![](https://lh3.googleusercontent.com/UZOy_3Bde6AiW0MeiOul5WMb7DU0-myeJOCj-H4hARaatn7rLlpjTubOMllcbR2FT7sWXLkIPPSnQ4dEj93uMqaHDhLR3sIr0CtnN3Lv1vLh7x0B0HdPBB8-NKSKXBopqbA4GtXo)

**Wait for 55 confirmations for DIN nodes to show up**

On vps Enter the command 

```bash
./sin-cli infinitynode mypeerinfo
```

If you are not getting an error and it still doesn't appear, on the console enter the `` infinitynode build-list`` command and wait.

:warning: This command will scan the entire chain from the beginning. In the meantime, your wallet may not respond. This process can take a few hours, depending on the number of transactions in your wallet. Please be patient.
