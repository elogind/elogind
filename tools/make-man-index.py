#!/usr/bin/env python3
#  -*- Mode: python; coding: utf-8; indent-tabs-mode: nil -*- */
# SPDX-License-Identifier: LGPL-2.1+
#
#  Copyright 2012 Lennart Poettering
#  Copyright 2013 Zbigniew Jędrzejewski-Szmek

import collections
import sys
import re
from xml_helper import xml_parse, xml_print, tree

MDASH = ' — ' if sys.version_info.major >= 3 else ' -- '

TEMPLATE = '''\
<refentry id="elogind.index" conditional="HAVE_PYTHON">

  <refentryinfo>
    <title>elogind.index</title>
    <productname>elogind</productname>

    <authorgroup>
      <author>
        <contrib>Developer</contrib>
        <firstname>Lennart</firstname>
        <surname>Poettering</surname>
        <email>lennart@poettering.net</email>
      </author>
      <!-- 1 /// Must add elogind authors for the additions and changes -->
      <author>
        <contrib>Developer</contrib>
        <firstname>Sven</firstname>
        <surname>Eden</surname>
        <email>sven.eden@gmx.de</email>
      </author>
      <!-- // 1 -->
    </authorgroup>
  </refentryinfo>

  <refmeta>
    <refentrytitle>elogind.index</refentrytitle>
    <manvolnum>7</manvolnum>
  </refmeta>

  <refnamediv>
    <refname>elogind.index</refname>
    <refpurpose>List all manpages from the elogind project</refpurpose>
  </refnamediv>
</refentry>
'''

SUMMARY = '''\
  <refsect1>
    <title>See Also</title>
    <para>
      <citerefentry><refentrytitle>elogind.directives</refentrytitle><manvolnum>7</manvolnum></citerefentry>
    </para>

    <para id='counts' />
  </refsect1>
'''

COUNTS = '\
This index contains {count} entries, referring to {pages} individual manual pages.'


def check_id(page, t):
    id = t.getroot().get('id')
    if not re.search('/' + id + '[.]', page):
        raise ValueError("id='{}' is not the same as page name '{}'".format(id, page))

def make_index(pages):
    index = collections.defaultdict(list)
    for p in pages:
        t = xml_parse(p)
        check_id(p, t)
        section = t.find('./refmeta/manvolnum').text
        refname = t.find('./refnamediv/refname').text
        purpose = ' '.join(t.find('./refnamediv/refpurpose').text.split())
        for f in t.findall('./refnamediv/refname'):
            infos = (f.text, section, purpose, refname)
            index[f.text[0].upper()].append(infos)
    return index

def add_letter(template, letter, pages):
    refsect1 = tree.SubElement(template, 'refsect1')
    title = tree.SubElement(refsect1, 'title')
    title.text = letter
    para = tree.SubElement(refsect1, 'para')
    for info in sorted(pages, key=lambda info: str.lower(info[0])):
        refname, section, purpose, realname = info

        b = tree.SubElement(para, 'citerefentry')
        c = tree.SubElement(b, 'refentrytitle')
        c.text = refname
        d = tree.SubElement(b, 'manvolnum')
        d.text = section

        b.tail = MDASH + purpose # + ' (' + p + ')'

        tree.SubElement(para, 'sbr')

def add_summary(template, indexpages):
    count = 0
    pages = set()
    for group in indexpages:
        count += len(group)
        for info in group:
            refname, section, purpose, realname = info
            pages.add((realname, section))

    refsect1 = tree.fromstring(SUMMARY)
    template.append(refsect1)

    para = template.find(".//para[@id='counts']")
    para.text = COUNTS.format(count=count, pages=len(pages))

def make_page(*xml_files):
    template = tree.fromstring(TEMPLATE)
    index = make_index(xml_files)

    for letter in sorted(index):
        add_letter(template, letter, index[letter])

    add_summary(template, index.values())

    return template

if __name__ == '__main__':
    with open(sys.argv[1], 'wb') as f:
        f.write(xml_print(make_page(*sys.argv[2:])))
