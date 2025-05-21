# elf2oux

## Opis

Program konwertuje plik w formacie ELF zbudowany w systemie Linux przy użyciu sandardowego ‘kompilatora’ do formatu uruchamianego w systemie operacyjnym OUX/C+. Z pliku o nazwie zakończonej “.elf” jest tworzony plik o podstawowej nazwie.

## Specyfikacja wyjściowego formatu OUXEXE

W pliku kolejno znajdują się:
* ciąg znaków “OUXEXE”
* przesunięcie do ‹symboli importowanych›
* przesunięcie do ‹symboli eksportowanych›
* przesunięcie do ‹nazw symboli›
* przesunięcie do sekcji “.got, po której następuje sekcja “.got.plt”
* przesunięcie do sekcji “.text”
* przesunięcie do sekcji “.data”
* przesunięcie do instrukcji startu programu
* 〈relokacje〉 z sekcji “.rela.dyn”
* 〈symbole importowane〉 z sekcji “.rela.plt”
* 〈symbole eksportowane〉 wybrane z sekcji “.dynsym”
* 〈nazwy symboli〉 z sekcji “.dynstr”
* sekcje “.got” i “.got.plt” (pierwsza wyrównana do 0x1000)
* sekcja “.text” (wyrównana do 0x1000)
* sekcja “.data” (wyrównana do 0x1000)

Wszystkie adresy VMA w danych (wartości relokacji, adresy symboli) są ustawione względem adresu wczytania 0.

## Specyfikacja wejściowego pliku ELF

Plik potrzebuje być zbudowany z użyciem dostosowanego skryptu ‘linkera’ dla architektury “x86_64”, by kolejno od adresu VMA 0 (po nagłówkach pliku) były sekcje:
* “.got” wyrównana do 0x1000
* “.got.plt” (w przyszłości powinna być wyrównana do 24?)
* “.text” wyrównana do 0x1000
* “.data” wyrównana do 0x1000

Pozostałe sekcje powinny być umieszczone na końcu.

Do sekcji “.data” należy przenieść zawartość sekcji “.rodata”.

Ponadto argumentem wiersza polecenia trzeba wyłączyć umieszczanie zmiennych w sekcji “.bss”.
