# Subnapi

Subnapi is a Linux program for automatic downloading and uploading of movie
subtitles. It matches subtitles against the local movie file (by hash, not by
file name) using online databases: [NapiProjekt](https://www.napiprojekt.pl),
[OpenSubtitles](https://www.opensubtitles.org) and
[Napisy24](https://napisy24.pl).

Subnapi is a fork of [QNapi](https://github.com/QNapi/qnapi), whose upstream
has been inactive since 2022. Version 0.5.0 is the first Subnapi release.

## What's different from the original QNapi

**Added**

* NapiProjekt subtitle **upload and correction** support (removed from QNapi
  in 0.2.2 when the old API died), reimplemented against the current
  NapiProjekt API:
  * an upload dialog for new subtitles and corrections (GUI),
  * a directory scan-and-review flow: scan a folder for movies with local
    subtitles, see which are missing from NapiProjekt's database and send the
    selected ones in one batch (GUI),
  * `-u/--upload` and `-r/--correct` switches (with `-a/--author`,
    `-m/--comment`) in the command-line client `subnapic`.
* `napiprojekt:` URI scheme handler — clicking a `napiprojekt:` link in a
  browser downloads subtitles by hash.
* Desktop notifications (DBus, with tray fallback) with a settings toggle.
* Flatpak packaging and a reproducible Debian package build.

**Changed**

* Ported from Qt5 to Qt6.
* Linux-only: Windows/macOS support, packaging and bundled prebuilt binaries
  were dropped.
* Configuration migrates automatically from `~/.config/qnapi.ini` to
  `~/.config/subnapi.ini` (the original file is left untouched).

**Fixed**

* UTF-16 LE subtitle processing.
* Encoding detection when downloading subtitles via `napiprojekt:` links.
* A temp-file collision that could make a `napiprojekt:` hash download return
  the content of an unrelated file.
* Download process not starting (pending upstream fix from QNapi PR #188).

## Installing

Prebuilt packages are available on the
[Releases](https://github.com/mskpluk/subnapi/releases) page.

### Debian / Ubuntu package

The `.deb` is built against Debian 13 "trixie" and installs cleanly on
Debian 13.x and Ubuntu 26.04 LTS:

```
$ sudo apt install ./subnapi_*_amd64.deb
```

### Flatpak

The `.flatpak` bundle works on any distribution with Flatpak set up; the
required runtime (`org.kde.Platform` 6.11) is fetched from Flathub
automatically:

```
$ flatpak install ./subnapi-*.flatpak
$ flatpak run io.github.mskpluk.subnapi
```

## Building from source

### Prerequisites

* a C++17 compiler (`g++` or `clang++`)
* Qt 6 with `qmake6` (Debian/Ubuntu: `qt6-base-dev qt6-base-dev-tools`)
* `libmediainfo-dev` and `pkgconf`
* at runtime: a 7-Zip binary (`7z`/`7za`; Debian/Ubuntu package `7zip` or
  `p7zip-full`) — subtitles from NapiProjekt are 7z-compressed

### Compiling

```
$ qmake6
$ make
```

This builds the GUI binary `subnapi` and the command-line client
`cli/subnapic` (append `CONFIG+=no_cli` or `CONFIG+=no_gui` to the qmake
invocation to skip one of them). `make install` respects `PREFIX`
(default `/usr`).

### Building the packages

The `.deb` builds reproducibly in a rootless Podman container pinned to
Debian 13.0 (no sudo needed beyond installing Podman itself); the package
lands in `../subnapi-deb-build/`:

```
$ scripts/build-deb-trixie.sh
```

Alternatively, on any Debian-based system with the build dependencies
installed: `dpkg-buildpackage -us -uc -b`.

The Flatpak builds and installs per-user from
`flatpak/io.github.mskpluk.subnapi.yml`; requires `flatpak` and
`flatpak-builder`. `BUNDLE=1` additionally creates a distributable
`.flatpak` bundle:

```
$ scripts/build-flatpak.sh
```

## Usage

Download subtitles by passing movie files as arguments (GUI or
`subnapic [files]`), through the tray icon menu, or by clicking
`napiprojekt:` links. Uploading requires a NapiProjekt account configured in
Settings. See `man subnapi` for all command-line options.

## License

GNU General Public License v2 or later. Subnapi contains code
Copyright (C) 2008-2017 Piotr Krzemiński and QNapi contributors.

## Issues

https://github.com/mskpluk/subnapi/issues
