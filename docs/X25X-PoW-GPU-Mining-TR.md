SIN (X25X Algo) T-Rex Miner Kullanarak Madencilik

1-  T-Rex Miner son sürümünü edinin

Nvidia grafik kartınıza uygun T-Rex Miner sürümünü indirin.

Cuda 10 versiyonu önerilir.

[https://github.com/trexminer/T-Rex/releases](https://github.com/trexminer/T-Rex/releases)

![](assets/img/x25x_pow_gpu_mining/1.png)

2-  Nvidia grafik kartı sürücünüzü güncelleyin.

Eğer grafik kartı sürücünüz güncel değilse, aşağıdaki linkten güncelemeyi indirmeniz tavsiye edilir.

[https://www.nvidia.com/Download/index.aspx?lang=en-us](https://www.nvidia.com/Download/index.aspx?lang=en-us)

![](assets/img/x25x_pow_gpu_mining/2.png)

  

3-  Örnek bir start.bat dosyası oluşturun.

T-Rex Miner bulunan klasöre gidiniz.

![](assets/img/x25x_pow_gpu_mining/3.png)

  
  
  
  

Bir text belgesi açınız.

Server adresini bulunduğunuz bölgeye göre seçiniz.

Avrupa sunucuları:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://eupool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

Asya sunucuları:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://asiapool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

A.B.D. sunucuları:

[t-rex](https://github.com/trexminer/T-Rex/releases) -a x25x -o stratum+tcp://uspool.sinovate.io:3253 -u <WALLET_ADDRESS>.%COMPUTERNAME% -p c=SIN

  

Biz bu örnekte Avrupa sunucusunu kullandık.

Lütfen kendi cüzdan adresinizi değiştirmeyi unutmayın!

![](assets/img/x25x_pow_gpu_mining/4.png)

Farklı Kaydet seçeneğiyle dosyanıza start.bat ismini vererek kaydedin.

![](assets/img/x25x_pow_gpu_mining/5.png)

4-  Madenciliğe başlayabilirz.

Lütfen oluşturduğunuz start.bat dosyasına çift tıklayınız. T-Rex seçtiğiniz sunucuya bağlanacak ve madencilik başlayacaktır.

  

![](assets/img/x25x_pow_gpu_mining/6.png)

T-Rex Miner'ın daha ayrıntılı kullanımı için lütfen [https://github.com/trexminer/T-Rex](https://github.com/trexminer/T-Rex) adresindeki Readme dosyasına bakın.
