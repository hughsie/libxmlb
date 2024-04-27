#!/usr/bin/python3
# pylint: disable=invalid-name,missing-docstring
#
# Copyright 2023 Chun-wei Fan <fanc999@yahoo.com.tw>
#
# SPDX-License-Identifier: LGPL-2.1-or-later

# Script to generate .def file from .map files

import os
import re
import sys


def get_sym_groups(lines, namespace):
    sym_groups = []
    search_header = True
    collect_symbols = False
    search_tail = False

    # namespace + version of library
    namespace_with_ver_regex = re.escape(namespace) + r"_[0-9]+\.[0-9]+\.[0-9]+"
    # Look for symbols added in version in library from map file,
    # which defines a group of symbols
    header_regex = re.compile(r"^" + namespace_with_ver_regex)
    # Look for where to start collecting symbols for the group
    global_regex = re.compile(r"^\s+global:\s+$")
    # Look for where to end collecting symbols for the group
    local_regex = re.compile(r"^\s+local:\s+\*+;\s+$")
    # Look, if any, since which version were the symbols
    # group added
    ending_notail_regex = re.compile(r"^};\s+$")
    ending_tail_regex = re.compile(r"^}\s+" + namespace_with_ver_regex + ";\s+$")

    for l in lines:
        # New symbols group found in map file
        if search_header and header_regex.match(l):
            symbols = []
            header = l.split()[0]
            search_header = False

        # Go through map file to gather up info for this symbols group
        elif not search_header:
            if not collect_symbols and global_regex.match(l):
                collect_symbols = True
            elif collect_symbols:
                tail = None

                # Stop gathering for group if `local: *;` is found
                if local_regex.match(l):
                    search_tail = True
                elif search_tail:
                    # Populate symbols group info and append
                    # to the groups we gathered previously
                    if ending_notail_regex.match(l) or ending_tail_regex.match(l):
                        if ending_tail_regex.match(l):
                            tail_ = l.split()[1]
                            tail = tail_[: tail_.index(";")]
                        sym_group = {
                            "header": header,
                            "symbols": symbols,
                            "tail": tail,
                        }
                        sym_groups.append(sym_group)
                        search_tail = False
                        collect_symbols = False
                        search_header = True

                # Gather up symbols for this group
                else:
                    symbol = l[: l.index(";")].strip()
                    symbols.append(symbol)

    return sym_groups


def output_def_file(def_file, sym_groups):
    with open(def_file, "w") as out:
        out.write("EXPORTS\n")
        for group in sym_groups:
            out.write("\n")
            if group["tail"] is not None:
                out.write(
                    "; APIs added in %s since %s\n" % (group["header"], group["tail"])
                )
            else:
                out.write("; APIs added in %s\n" % group["header"])
            for symbol in group["symbols"]:
                out.write("%s\n" % symbol)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("%s <mapfile> <namespace> <deffile>" % sys.argv[0])
    mapfile = sys.argv[1]
    namespace = sys.argv[2]
    def_file = sys.argv[3]
    with open(mapfile, "r") as m:
        lines = m.readlines()
        groups = get_sym_groups(lines, namespace)
        output_def_file(def_file, groups)
