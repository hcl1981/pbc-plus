# Prueft Spielmechanik, Renderer und Tonausgabe auf dem Entwicklungsrechner.
# Die echten Quelldateien aus ../src werden gegen Stub-Header uebersetzt.
#   make        alle Tests laufen lassen
#   make clean
SRC = ../src
CXXFLAGS = -O1 -Ishim -I$(SRC) -Wall -Wno-unused-variable

all: run

test_game: test_game.cpp $(SRC)/game.cpp $(SRC)/render.cpp $(SRC)/assets_level.cpp $(SRC)/assets_gfx.cpp $(SRC)/assets_title.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lz

# audio.cpp wird per #include eingebunden, muss aber Abhaengigkeit sein.
test_audio: test_audio.cpp $(SRC)/audio.cpp $(SRC)/assets_snd.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_audio.cpp $(SRC)/assets_snd.cpp

run: test_game test_audio
	./test_game
	@echo
	./test_audio
	@echo
	@echo "Bilder: view_1x.png view_2x.png view_mask.png"
	@echo "Toene:  music.wav sfx.wav mixed.wav"

clean:
	$(RM) test_game test_audio *.png *.wav

screenshots: screenshots.cpp $(SRC)/game.cpp $(SRC)/render.cpp $(SRC)/assets_level.cpp $(SRC)/assets_gfx.cpp $(SRC)/assets_title.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ -lz

shots: screenshots
	./screenshots
