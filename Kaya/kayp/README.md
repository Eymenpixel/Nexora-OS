# Kayp - Kaya OS Package Manager

**Güçlü ama Basit.** Tek binary, dpkg benzeri, sıfırdan C++17 yazılmış.

## Özellikler

- ✅ `.kaya` paket formatı (tar tabanlı)
- ✅ Paket kurma ve kaldırma
- ✅ Veritabanı yönetimi (`/var/lib/kayp/status`)
- ✅ Pre/post install scriptleri
- ✅ Shell injection koruması
- ✅ Minimal bağımlılık (sadece tar, tar.gz desteği)
- ✅ `--root` seçeneği ile fakeroot kurulu

## Kullanım

### Kurulum

```bash
make
make install
```

### Komutlar

```bash
# Paket kur
kayp install paket.kaya

# Paket kaldır
kayp remove paket-adi

# Kurulu paketleri listele
kayp list

# Paket bilgisini göster
kayp info paket.kaya
```

### Fakeroot ile Derleme

```bash
# Alternatif root'a kurulum
kayp install paket.kaya --root /tmp/fakeroot
kayp list --root /tmp/fakeroot
```

## .kaya Paket Formatı

`.kaya` dosyası bir tar arsividir ve içinde şunlar yer alır:

```
control          (Meta bilgi: Name, Version, Description, Depends)
data.tar.gz      (Kurulacak dosyalar - root'a göre relatif)
preinst          (Kurulumdan önce çalışan script - opsiyonel)
postinst         (Kurulumdan sonra çalışan script - opsiyonel)
prerm            (Kaldırımdan önce çalışan script - opsiyonel)
postrm           (Kaldırımdan sonra çalışan script - opsiyonel)
```

### Control Dosyası Örneği

```
Name: cowsay
Version: 3.0.4
Description: Konuşan inek programı
Depends: perl
```

## Mimari

- **main.cpp** - CLI frontend
- **package.cpp/hpp** - Kurulum motoru
  - tar arsivi açma
  - Script yürütme
  - Dosya yönetimi
- **db.cpp/hpp** - Durum veritabanı
  - Kurulu paketleri takip etme
  - `/var/lib/kayp/status` yönetimi
- **control_parser.cpp/hpp** - Metadata parser
  - "Field: Value" formatını parse etme

## Veritabanı Format

`/var/lib/kayp/status` dosyası kurulu paketleri tutar:

```
Name: cowsay
Version: 3.0.4
Description: Konuşan inek programı
  /usr/bin/cowsay
  /usr/share/man/man1/cowsay.1

Name: perl
Version: 5.28
Description: Perl programlama dili
  /usr/bin/perl
  /usr/lib/perl/5.28
```

## Özel Özellikler

### RAII Temp Directories
Geçici çalışma dizinleri otomatik temizlenir (RAII pattern).

### Shell Injection Koruması
Tüm shell komutları `shell_quote()` ile escape edilir.

### Dosya Listesi Takibi
Kaldırıldığında tüm kurulan dosyalar ters sırada silinir.

### Ön/Son Script Desteği
Pre/post install ve pre/post remove scriptleri çalıştırılır.

## Derlemek

```bash
cd kayp-new
make          # Derle
make install  # /usr/bin/kayp'a yükle
make clean    # Temizle
```

## Sistem Gereksinimleri

- GCC 7+ veya Clang (C++17 desteği)
- GNU tar (gz desteği)
- POSIX-uyumlu sistem (Linux, BSD, vb.)

## Lisans

GNU GPL - Kaya OS ile uyumlu
