# komunikator-okno: okno w Qt5 dla silnika "komunikator". Osobno od silnika - silnik buduje się bez Qt.
# Potrzebne do budowy (Ubuntu): sudo apt install g++ make qtbase5-dev libqt5svg5
CXX      ?= g++
QTBIN    := $(shell pkg-config --variable=host_bins Qt5Core)
MOC      := $(QTBIN)/moc
RCC      := $(QTBIN)/rcc
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
CXXFLAGS += -fPIC $(shell pkg-config --cflags Qt5Widgets Qt5Network)
LIBS     := $(shell pkg-config --libs Qt5Widgets Qt5Network)
PREFIX   ?= $(HOME)/.local
B        := build

ZRODLA   := main.cpp silnik.cpp motyw.cpp widzety.cpp okno.cpp
Z_MOC    := silnik.hpp widzety.hpp okno.hpp
OBIEKTY  := $(ZRODLA:%.cpp=$(B)/%.o) $(Z_MOC:%.hpp=$(B)/moc_%.o) $(B)/zasoby.o

komunikator-okno: $(OBIEKTY)
	$(CXX) -o $@ $^ $(LIBS)

$(B)/%.o: %.cpp $(wildcard *.hpp) | $(B)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(B)/moc_%.cpp: %.hpp | $(B)
	$(MOC) -o $@ $<

$(B)/moc_%.o: $(B)/moc_%.cpp
	$(CXX) $(CXXFLAGS) -I. -c -o $@ $<

$(B)/zasoby.cpp: zasoby.qrc styl.qss $(shell find zasoby -type f) | $(B)
	$(RCC) -name zasoby -o $@ $<

$(B)/zasoby.o: $(B)/zasoby.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(B):
	mkdir -p $(B)

# Zrzuty ekranów bez monitora (do porównania z projektem): make zrzuty, wynik w build/zrzuty/
$(B)/robzrzuty: testy/zrzuty.cpp $(filter-out $(B)/main.o,$(OBIEKTY))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

zrzuty: $(B)/robzrzuty
	bash testy/zrzuty.sh

install: komunikator-okno
	install -Dm755 komunikator-okno $(DESTDIR)$(PREFIX)/bin/komunikator-okno
	install -Dm644 zasoby/app/komunikator.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/komunikator-okno.svg
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	sed "s|@BIN@|$(PREFIX)/bin/komunikator-okno|" komunikator-okno.desktop \
	  > $(DESTDIR)$(PREFIX)/share/applications/komunikator-okno.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/komunikator-okno \
	      $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/komunikator-okno.svg \
	      $(DESTDIR)$(PREFIX)/share/applications/komunikator-okno.desktop

clean:
	rm -rf $(B) komunikator-okno

.PHONY: install uninstall clean zrzuty
