# komunikator - budowa bez zewnętrznych bibliotek (tylko kompilator C++17 i biblioteka standardowa).
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
PREFIX   ?= $(HOME)/.local

ZRODLA   = src/main.cpp src/wyoming.cpp src/json.cpp src/wav.cpp src/gemini.cpp src/proces.cpp
NAGLOWKI = $(wildcard src/*.hpp)

komunikator: $(ZRODLA) $(NAGLOWKI)
	$(CXX) $(CXXFLAGS) -o $@ $(ZRODLA)

# Jeden plik bez żadnych bibliotek dzielonych - da się skopiować na inny komputer z Linuksem.
statyczny: $(ZRODLA) $(NAGLOWKI)
	$(CXX) $(CXXFLAGS) -static -o komunikator $(ZRODLA)

# Okno (Qt5) - osobny program, osobne zależności; silnik dalej buduje się bez Qt.
okno:
	$(MAKE) -C okno

okno-install: okno
	$(MAKE) -C okno install

test: komunikator
	bash testy/testy.sh

# Kontrola jakości: ostrzeżenia jako błędy oraz cały zestaw testów na kompilacji z sanitizerami
# (AddressSanitizer: błędy pamięci, UndefinedBehaviorSanitizer: zachowania niezdefiniowane).
SANFLAGS = -O1 -g -std=c++17 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer \
           -fsanitize=address,undefined -fno-sanitize-recover=all

build/komunikator-san: $(ZRODLA) $(NAGLOWKI)
	@mkdir -p build
	$(CXX) $(SANFLAGS) -o $@ $(ZRODLA)

sprawdz: build/komunikator-san
	$(CXX) $(CXXFLAGS) -Werror -fsyntax-only $(ZRODLA)
	KOMUNIKATOR_BIN=$(CURDIR)/build/komunikator-san bash testy/testy.sh

install: komunikator
	install -Dm755 komunikator $(DESTDIR)$(PREFIX)/bin/komunikator

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/komunikator

clean:
	rm -f komunikator
	rm -rf build

.PHONY: statyczny okno okno-install test sprawdz install uninstall clean
