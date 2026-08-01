OUTDIR = $${PWD}
isEmpty(PREFIX) {
    INSTALL_PREFIX = /usr
} else {
    INSTALL_PREFIX = $${PREFIX}
}
