TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = libqnapi cli gui
no_cli:message(will skip building subnapic cli application)
no_cli:SUBDIRS -= cli
no_gui:message(will skip building subnapi gui application)
no_gui:SUBDIRS -= gui
TRANSLATIONS += translations/subnapi_it.ts translations/subnapi_pl.ts
include(qnapi.pri)
unix {
    DATADIR=$${INSTALL_PREFIX}/share
    doc.path = $${INSTALL_PREFIX}/share/doc/subnapi
    doc.files = doc/ChangeLog \
        doc/LICENSE \
        doc/LICENSE-pl \
        doc/COPYRIGHT \
        doc/subnapi-download.desktop \
        doc/subnapi-scan.desktop \
        doc/subnapi-download.schemas \
        doc/subnapi-scan.schemas
    man.path = $${INSTALL_PREFIX}/share/man/man1/
    man.files = doc/man/subnapi.1.gz
    man_it.path = $${INSTALL_PREFIX}/share/man/it/man1/
    man_it.files = doc/man/it/subnapi.1.gz
    man_pl.path = $${INSTALL_PREFIX}/share/man/pl/man1/
    man_pl.files = doc/man/pl/subnapi.1.gz
    icons16.files = gui/res/icons/16x16/apps/subnapi.png
    icons16.path = $${DATADIR}/icons/hicolor/16x16/apps/
    icons32.files = gui/res/icons/32x32/apps/subnapi.png
    icons32.path = $${DATADIR}/icons/hicolor/32x32/apps/
    icons48.files = gui/res/icons/48x48/apps/subnapi.png
    icons48.path = $${DATADIR}/icons/hicolor/48x48/apps/
    icons128.files = gui/res/icons/128x128/apps/subnapi.png
    icons128.path = $${DATADIR}/icons/hicolor/128x128/apps/
    icons512.files = gui/res/icons/512x512/apps/subnapi.png
    icons512.path = $${DATADIR}/icons/hicolor/512x512/apps/
    INSTALLS += icons16 icons32 icons48 icons128 icons512
    desktop.path = $${INSTALL_PREFIX}/share/applications
    desktop.files = doc/subnapi.desktop doc/subnapi-uri.desktop
    INSTALLS += doc man man_it man_pl desktop

    # NapiProjekt mime type
    napimime.path = $${INSTALL_PREFIX}/share/subnapi
    napimime.files = doc/napiprojekt-mime.xml
    INSTALLS += napimime
}
doxygen.commands = rm -fr doc/doxygen/* && doxygen Doxyfile && echo "Doxygen documentation generated in doc/doxygen/index.html"
QMAKE_EXTRA_TARGETS += doxygen

# Target do budowania paczki deb
deb.commands = \
    rm -rf subnapi-package && \
    mkdir -p subnapi-package/DEBIAN && \
    mkdir -p subnapi-package/usr/bin && \
    mkdir -p subnapi-package/usr/share/applications && \
    mkdir -p subnapi-package/usr/share/doc/subnapi && \
    mkdir -p subnapi-package/usr/share/subnapi && \
    mkdir -p subnapi-package/usr/share/icons/hicolor/16x16/apps && \
    mkdir -p subnapi-package/usr/share/icons/hicolor/32x32/apps && \
    mkdir -p subnapi-package/usr/share/icons/hicolor/48x48/apps && \
    mkdir -p subnapi-package/usr/share/icons/hicolor/128x128/apps && \
    mkdir -p subnapi-package/usr/share/icons/hicolor/512x512/apps && \
    cp debian/control subnapi-package/DEBIAN/control && \
    cp debian/subnapi.postinst subnapi-package/DEBIAN/postinst && \
    cp debian/subnapi.postrm subnapi-package/DEBIAN/postrm && \
    chmod +x subnapi-package/DEBIAN/postinst && \
    chmod +x subnapi-package/DEBIAN/postrm && \
    cp subnapi subnapi-package/usr/bin/subnapi && \
    cp doc/subnapi.desktop subnapi-package/usr/share/applications/ && \
    cp doc/subnapi-uri.desktop subnapi-package/usr/share/applications/ && \
    cp doc/COPYRIGHT subnapi-package/usr/share/doc/subnapi/copyright && \
    cp doc/napiprojekt-mime.xml subnapi-package/usr/share/subnapi/ && \
    cp gui/res/icons/16x16/apps/subnapi.png subnapi-package/usr/share/icons/hicolor/16x16/apps/ && \
    cp gui/res/icons/32x32/apps/subnapi.png subnapi-package/usr/share/icons/hicolor/32x32/apps/ && \
    cp gui/res/icons/48x48/apps/subnapi.png subnapi-package/usr/share/icons/hicolor/48x48/apps/ && \
    cp gui/res/icons/128x128/apps/subnapi.png subnapi-package/usr/share/icons/hicolor/128x128/apps/ && \
    cp gui/res/icons/512x512/apps/subnapi.png subnapi-package/usr/share/icons/hicolor/512x512/apps/ && \
    dpkg-deb --build --root-owner-group subnapi-package subnapi-package.deb && \
    echo "Package `dpkg-deb -f subnapi-package.deb Package`_`dpkg-deb -f subnapi-package.deb Version`_`dpkg-deb -f subnapi-package.deb Architecture`.deb created successfully" && \
    mv subnapi-package.deb `dpkg-deb -f subnapi-package.deb Package`_`dpkg-deb -f subnapi-package.deb Version`_`dpkg-deb -f subnapi-package.deb Architecture`.deb && \
    rm -rf subnapi-package
QMAKE_EXTRA_TARGETS += deb
