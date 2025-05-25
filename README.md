### Dosya-Sistemi-Simulatoru

**Kurulum (Derleme):** Projeyi derlemek için Makefile bulunmaktadır. Aşağıdaki komut ile derleme yapabilirsiniz:
* make
* Bu komut, C derleyicisi (gcc) kullanarak fs.c ve main.c dosyalarını derler ve simplefs adlı çalıştırılabilir programı oluşturur.

**Kullanım:** Derleme tamamlandıktan sonra programı çalıştırmak için:
* ./simplefs

Program çalıştırıldığında 1 MB boyutunda bir sanal disk dosyası disk.sim mevcut dizinde oluşturulur (eğer yoksa) ve dosya sistemi yüklenir. Program, konsol tabanlı bir menü arayüzü sunar. Menüden seçim yaparak dosya sistemi komutlarını kullanabilirsiniz.

Mevcut menü seçenekleri ve işlevleri:

* 1.Dosya oluştur - Yeni bir dosya oluşturur.
* 2.Dosya sil - Mevcut bir dosyayı siler.
* 3.Dosyaya yaz - Dosya içine veri yazar (dosya yoksa hata verir).
* 4.Dosyadan oku - Dosya içerisinden belirli bir konumdan itibaren veri okur.
* 5.Dosyaları listele - Tüm dosyaların isim ve boyut bilgilerini listeler.
* 6.Diski formatla - Tüm dosyaları siler, dosya sistemini sıfırlar.
* 7.Dosyayı yeniden adlandır - Bir dosyanın adını değiştirir.
* 8.Dosya var mı (ara) - Belirtilen isimde bir dosya var mı kontrol eder.
* 9.Dosya boyutu - Dosyanın boyutunu (byte) gösterir.
* 10.Dosyaya ekle - Dosyanın sonuna veri ekler.
* 11.Dosyayı kısalt - Dosyanın boyutunu küçültür (içerikten keserek).
* 12.Dosya kopyala - Bir dosyanın içeriğini yeni bir dosyaya kopyalar.
* 13.Dosya taşı - Bir dosyayı başka bir konuma/isme taşır (bu projede yeniden adlandırma ile aynı).
* 14.Birleştir (Defragment) - Diskteki boş alanları birleştirir, parçalı verileri düzenler.
* 15.Bütünlük kontrolü - Dosya sistemi tutarlılığını kontrol eder (metadata ve veri blokları).
* 16.Disk yedeğini al - Tüm disk.sim dosyasını belirtilen isimle yedekler.
* 17.Disk yedeğinden dön - Belirtilen yedek dosyasından disk durumunu geri yükler.
* 18.Dosya içeriğini görüntüle (cat) - Dosyanın tüm içeriğini ekrana yazdırır.
* 19.İki dosyayı karşılaştır (diff) - İki dosyanın içeriklerini karşılaştırır ve farklılık varsa bildirir.
* 20.İşlem günlüğünü göster (log) - Dosya sistemi üzerinde yapılan işlemlerin günlüğünü gösterir.
* 21.Çıkış - Programdan çıkar.


Komut satırında, program ilgili seçenek için sizden gerekli bilgileri isteyecektir (dosya adı, veri, boyut gibi). Örneğin `Dosyaya yaz` seçeneği için dosya adını ve yazılacak veriyi girmeniz istenir. `Dosyadan oku` için dosya adı, başlangıç ofseti ve okunacak byte sayısı girilir.

**Örnek Kullanım:**
- *Dosya oluşturma:* Menüden *1* seçeneği ile dosya adı sorulur. Örneğin "deneme.txt" girildiğinde, eğer aynı isimde bir dosya yoksa dosya oluşturulur.
- *Dosyaya yazma:* Menüden *3* seçilerek "deneme.txt" dosyasına yazmak istediğiniz metni girin. Örneğin "Hello World" girildiğinde dosyaya bu içerik yazılır ve dosya boyutu 11 bayt olur.
- *Dosyadan okuma:* Menüden *4* seçilerek "deneme.txt" dosyasından, örneğin ofset 0'dan 5 bayt okunması istendiğinde ekranda "Hello" çıktısı görülür.
- *Dosyaları listeleme: *5* seçilerek şu an diskte bulunan dosyalar listelenir.
- *Yedek alma ve geri yükleme:* *16* seçeneği ile örneğin "disk_yedek.sim" adıyla disk yedeği oluşturulabilir. *17* seçeneği ile bu yedekten geri yükleme yapılabilir.
- *Birleştirme (defragment):* Zamanla dosya silme ve yazma işlemleri sonrasında disk içinde parçalanmalar oluşursa *14* seçeneği ile disk birleştirilerek dosya blokları bitişik hale getirilir, boş alanlar tek parça toplanır.
- *İşlem günlüğü:* Program çalıştığı sürece yapılan tüm işlemler *fs.log* isimli bir günlük dosyasına kaydedilir. *20* seçeneği ile bu log dosyasının içeriği görüntülenebilir. Örneğin bir dosya oluşturduğunuzda veya sildiğinizde tarih/saat ile birlikte log kaydı tutulur.

**Notlar:**
- Program ilk çalıştığında disk.sim dosyası bulunmazsa otomatik olarak oluşturur ve boş halde başlatılır. 
- fs_format komutu (seçenek 6) disk.sim dosyasını tamamen sıfırlar (1 MB'lık sanal diskin içini temizler) ve metadata bölümünü temizler. Bu işlemin geri dönüşü yoktur, disk içindeki tüm sanal dosyalar silinir.
- Maksimum dosya sayısı **64**'tür. Bu limite ulaşıldığında yeni dosya oluşturulamaz.
- Aynı ada sahip birden fazla dosya oluşturulması engellenmiştir.
- Dosya ismi olarak en fazla **32** karakter kullanılabilir.
- Disk boyutu sabit 1 MB olarak tanımlıdır. Dosya ekleme veya yazma işlemlerinde yeterli boş alan yoksa veya boş alan disk içinde parça parça dağılmış ise (fragmentation), işlem başarısız olabilir.
- Dosya zaman bilgisi olarak yalnızca **oluşturulma tarihi** saklanmaktadır. Log kayıtlarında sistem saati kullanılır.
- İşlem günlüğü dosyası fs.log, program kapansa bile dizinde kalır. 

