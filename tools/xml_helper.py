#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

from lxml import etree as tree


class CustomResolver(tree.Resolver):
    def resolve(self, url, _id, context):
        if "custom-entities.ent" in url:
            return self.resolve_filename("man/custom-entities.ent", context)
        if "ethtool-link-mode" in url:
            return self.resolve_filename("src/shared/ethtool-link-mode.xml", context)

        return None


_parser = tree.XMLParser(resolve_entities=True)
# resolve_entities (XML_PARSE_NOENT) is required so that libxml2 loads the
# external parameter entity custom-entities.ent referenced via %entities;
# in the internal DTD subset of some pages (e.g. man/elogind.syntax.xml,
# man/logind.conf.xml). Without it the CustomResolver above is never
# consulted and parsing fails with "Entity 'entities' not defined".
# As a side effect, general entities (e.g. &KILL_USER_PROCESSES;) are
# substituted with their values, which is what the index generators expect.
# pylint: disable=no-member
_parser.resolvers.add(CustomResolver())


def xml_parse(page):
    doc = tree.parse(page, _parser)
    doc.xinclude()
    return doc


def xml_print(xml):
    return tree.tostring(xml, pretty_print=True, encoding="utf-8")
