# Scuffedaim private server client 

This is a fork of neosu which is a third-party fork of [McOsu](https://store.steampowered.com/app/607260/McOsu/), unsupported by McKay.

If you need help, contact `sneznykocur` on Discord

### Building (windows)

Make sure you pulled Git LFS files (method varies depending on your Git client).

To run neosu, open `neosu.vcxproj` with Visual Studio then press F5.

### Building (linux)

Required dependencies:

- [bass](https://www.un4seen.com/download.php?bass24-linux)
- [bassmix](https://www.un4seen.com/download.php?bassmix24-linux)
- [bass_fx](https://www.un4seen.com/download.php?z/0/bass_fx24-linux)
- [BASSloud](https://www.un4seen.com/download.php?bassloud24-linux)
- blkid
- freetype2
- glew
- libjpeg
- liblzma
- xi
- zlib

On Debian, this should be:
```
apt install pkgconf git-lfs libcurl4-openssl-dev libfreetype-dev libblkid-dev libglew-dev libjpeg-dev liblzma-dev libxi-dev
```

Once those are installed, just run `make -j8`.
