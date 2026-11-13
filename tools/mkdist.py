#!/usr/bin/env python3
# mkdist.py - builds the Aminet archive PixooLib.lha from the built tree
#
# Copyright (c) 2026 André Gewert <agewert@ubergeek.de>, MIT License
#
# Creates dist/PixooLib/ with the library, PixooCtl, the demo, the Aminet
# readme (English and German; the README*.md only live in Source/), the
# developer files (headers, fd, sfd and the AmigaGuide reference of the
# public headers, build/pixoo.library.guide from "make guide") and the
# sources, gives the files classic Workbench icons, then packs it with lha.
# Common parts come from amiga-devkit/tools/distlib.py. Run "make dist"
# (needs doxygen and jlha or lha). Amiga browsers cannot show Doxygen's
# HTML, so the archive has no HTML reference.

import os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'amiga-devkit', 'tools'))
from amigaicons import WBDRAWER, WBTOOL, WBPROJECT, write_icon  # noqa: E402
from distlib import copy, copytree, copy_devkit, pack  # noqa: E402

NAME = 'PixooLib'
DIST = os.path.join(ROOT, 'dist')
PKG = os.path.join(DIST, NAME)
MULTIVIEW = 'SYS:Utilities/MultiView'


def main():
    b = lambda *p: os.path.join(ROOT, *p)
    p = lambda *q: os.path.join(PKG, *q)
    shutil.rmtree(DIST, ignore_errors=True)
    os.makedirs(PKG)

    copy(b('package', NAME + '.readme'), p(NAME + '.readme'))
    copy(b('package', 'Install'), p('Install'))
    copy(b('LICENSE'), p('LICENSE'))
    copy(b('bin', 'pixoo.library'), p('Libs', 'pixoo.library'))
    copy(b('bin', 'PixooCtl'), p('PixooCtl'))
    copytree(b('demo'), p('Demo'))

    dev = p('Developer')
    for d in ('libraries', 'clib', 'proto', 'inline'):
        copytree(b('include', d), os.path.join(dev, 'include', d))
    copytree(b('include', 'fd'), os.path.join(dev, 'fd'))
    copytree(b('sfd'), os.path.join(dev, 'sfd'))
    copy(b('build', 'pixoo.library.guide'), os.path.join(dev, 'pixoo.library.guide'))

    # sources in the layout of the repository, so "make" works there
    src = p('Source')
    for d in ('src', 'include', 'sfd', 'test', 'tools', 'demo', 'package'):
        copytree(b(d), os.path.join(src, d), ignore=('__pycache__', 'hosttest'))
    for f in ('Makefile', 'README.md', 'README.de.md', 'CLAUDE.md', 'LICENSE', '.gitignore',
              '.editorconfig'):
        if os.path.exists(b(f)):
            copy(b(f), os.path.join(src, f))
    copy_devkit(os.path.join(src, 'amiga-devkit'))

    # classic icons only (no style sets): this project keeps them simple
    write_icon(PKG, 'Classic', 'drawer', WBDRAWER)
    icons = [('Install', 'install', WBPROJECT,
              dict(default_tool='Installer', tooltypes=('APPNAME=pixoo.library', 'MINUSER=AVERAGE'))),
             ('PixooCtl', 'tiles', WBTOOL, {}),
             (NAME + '.readme', 'readme', WBPROJECT, dict(default_tool=MULTIVIEW)),
             ('LICENSE', 'license', WBPROJECT, dict(default_tool=MULTIVIEW)),
             ('Developer/pixoo.library.guide', 'readme', WBPROJECT, dict(default_tool=MULTIVIEW))]
    for d in ('Libs', 'Demo', 'Demo/Pictures', 'Developer', 'Source'):
        icons.append((d, 'drawer', WBDRAWER, {}))
    for f in sorted(os.listdir(p('Demo', 'Pictures'))):
        icons.append(('Demo/Pictures/' + f, 'tiles', WBPROJECT, dict(default_tool=MULTIVIEW)))
    for path, role, kind, kw in icons:
        write_icon(p(path), 'Classic', role, kind, **kw)

    pack(DIST, NAME)


if __name__ == '__main__':
    main()
