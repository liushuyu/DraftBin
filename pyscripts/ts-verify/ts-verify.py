#! /usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Simple XML validator done while learning the use of lxml library.
#   -- Juhamatti Niemelä <iiska AT iki DOT fi>
# Modified by liushuyu <bGl1c2h1eXUwMTFAZ21haWwuY29tCg==>

import lxml
from lxml import etree
import sys
# import os


def main():
    if len(sys.argv) < 2:
        print("Usage: %s translaton_file.ts ..." % (sys.argv[0]))
        exit(0)
    try:
        with open('ts-verify.xsd') as f:
            doc = etree.parse(f)
    except FileNotFoundError:
        print('\033[91mERROR:\033[0m Please place `ts-verify.xsd\' to the same directory as this script!')
        sys.exit(1)

    print("Validating schema file ... ")
    try:
        schema = etree.XMLSchema(doc)
    except lxml.etree.XMLSchemaParseError as e:
        print(e)
        exit(1)

    print("Schema OK")

    for doc_file in sys.argv[1:]:
        try:
            with open(doc_file) as f:
                try:
                    doc = etree.parse(f)
                except Exception as e:
                    error_msg(doc_file, e)
        except FileNotFoundError:
            print('\033[91mERROR:\033[0m File not found: {}'.format(doc_file))
            sys.exit(1)
        print("Validating translation: {} ...".format(doc_file))
        try:
            schema.assertValid(doc)
        except lxml.etree.DocumentInvalid as e:
            error_msg(doc_file, e)
        except lxml.etree.XMLSyntaxError as e:
            error_msg(doc_file, e)

        print("\033[32mNo error found\033[0m: {}".format(doc_file))


def error_msg(doc_file, e):
    print('\033[91mERROR:\033[0m  In file {}:\n\t{}'.format(
        doc_file, e))
    print('To prevent from false-positive, process terminated...')
    exit(1)

if __name__ == "__main__":
    main()
