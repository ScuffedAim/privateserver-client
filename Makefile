TARGET = build/neosu
SOURCES = $(shell find src -type f -name '*.cpp')
OBJECTS = $(patsubst src/%.cpp, obj/%.o, $(SOURCES))
HEADERS = $(shell find src -type f -name '*.h')

LIBS = blkid freetype2 glew libjpeg liblzma xi zlib

CXXFLAGS = -std=c++2a -fmessage-length=0 -fno-exceptions -Wno-sign-compare -Wno-unused-local-typedefs -Wno-reorder -Wno-switch -Wall
CXXFLAGS += `pkgconf --static --cflags $(LIBS)` `curl-config --cflags`
CXXFLAGS += -Isrc/App -Isrc/App/Osu -Isrc/Engine -Isrc/GUI -Isrc/GUI/Windows -Isrc/GUI/Windows/VinylScratcher -Isrc/Engine/Input -Isrc/Engine/Platform -Isrc/Engine/Main -Isrc/Engine/Renderer -Isrc/Util
CXXFLAGS += -Ilibraries/bass/include -Ilibraries/bassasio/include -Ilibraries/bassfx/include -Ilibraries/bassmix/include -Ilibraries/basswasapi/include -Ilibraries/bassloud/include -Ilibraries/discord-sdk/include
CXXFLAGS += -O2 -g3 -fsanitize=address

LDFLAGS = -lbass -lbassmix -lbass_fx -lbassloud -lstdc++
LDFLAGS += `pkgconf --static --libs $(LIBS)` `curl-config --static-libs --libs`


$(TARGET): build $(OBJECTS) $(HEADERS)
	@echo "CC" $(TARGET)
	@$(CXX) -o $(TARGET) $(OBJECTS) $(CXXFLAGS) $(LDFLAGS)

obj/%.o: src/%.cpp
	@echo "CC" $<
	@mkdir -p $(dir $@)
	@$(CXX) -c $< -o $@ $(CXXFLAGS)

.PHONY: build clean ensure-lfs
build: ensure-lfs
	cp -nr resources build

clean:
	rm -rf build obj

ensure-lfs:
	@if [ ! -f .git/hooks/pre-push ]; then \
		echo "Git LFS not installed. Installing..."; \
		git lfs install; \
	fi
	@if git lfs ls-files | grep -q '^[a-f0-9]\{64\} ' > /dev/null 2>&1; then \
		echo "Pulling Git LFS files..."; \
		git lfs pull; \
	fi

