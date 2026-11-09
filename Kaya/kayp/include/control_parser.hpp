// control_parser.hpp
// Kaya Package Manager (kayp) - .kaya paketlerinin "control" dosyasini okur.
// Format dpkg'nin control dosyasina benzer ama basitlestirilmis:
//
//   Name: cowsay
//   Version: 3.0.4
//   Description: Konusan inek programi
//   Depends: perl
//
#pragma once

#include <map>
#include <string>

namespace kayp {

// Bir control dosyasini "Alan: Deger" satirlarindan olusan bir map'e cevirir.
// Bos satirlar ve '#' ile baslayan satirlar yok sayilir.
std::map<std::string, std::string> parse_control(const std::string& path);

// Yardimci: map icinden bir alani guvenli sekilde okur, yoksa varsayilan doner.
std::string get_field(const std::map<std::string, std::string>& fields,
                       const std::string& key,
                       const std::string& def = "");

} // namespace kayp
