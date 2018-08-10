#!/usr/bin/env python3
#  -*- Mode: python; coding: utf-8; indent-tabs-mode: nil -*- */
#  SPDX-License-Identifier: LGPL-2.1+
#
#  Copyright © 2012-2013 Zbigniew Jędrzejewski-Szmek

from lxml import etree as tree

class CustomResolver(tree.Resolver):
    def resolve(self, url, id, context):
        if 'custom-entities.ent' in url:
            return self.resolve_filename('man/custom-entities.ent', context)

_parser = tree.XMLParser()
_parser.resolvers.add(CustomResolver())

def xml_parse(page):
    doc = tree.parse(page, _parser)
    doc.xinclude()
    return doc

def xml_print(xml):
    return tree.tostring(xml, pretty_print=True, encoding='utf-8')
