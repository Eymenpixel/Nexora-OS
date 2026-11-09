// package.hpp
// .kaya paket dosyalarini acar, kurar, kaldirir.
//
// .kaya dosyasi bir tar arsividir, icinde:
//   control          -> paket bilgisi (Name, Version, Description, Depends)
//   data.tar.gz      -> kurulacak dosyalar (root'a gore relatif yollarla)
//   preinst / postinst / prerm / postrm  -> opsiyonel, calistirilabilir script'ler
//
#pragma once

#include <string>
#include "db.hpp"

namespace kayp {

struct PackageInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string depends; // virgulle ayrilmis, simdilik sadece bilgi amacli gosteriliyor
};

// .kaya dosyasindan sadece control bilgisini okur (kurmadan once "info" icin).
PackageInfo read_package_info(const std::string& kaya_file);

// Paketi kurar. root: kurulacak hedef sistemin kok dizini ("/" ya da bir fakeroot).
// db: guncellenecek durum veritabani (kaydedilmesi cagiran tarafin sorumlulugunda).
// Basarili olursa true doner.
bool install_package(const std::string& kaya_file, const std::string& root, StatusDB& db);

// Kurulu bir paketi kaldirir (dosyalarini siler, db'den cikarir).
bool remove_package(const std::string& name, const std::string& root, StatusDB& db);

} // namespace kayp
