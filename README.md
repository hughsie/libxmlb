libxmlb
=======

Introduction
------------

XML is slow to parse and strings inside the document cannot be memory mapped as
they do not have a trailing NUL char. The libxmlb library takes XML source, and
converts it to a structured binary representation with a deduplicated string
table -- where the strings have the NULs included.

This allows an application to mmap the binary XML file, do an XPath query and
return some strings without actually parsing the entire document. This is all
done using (almost) zero allocations and no actual copying of the binary data.

As each node in the binary XML file encodes the 'next' node at the same level
it makes skipping whole subtrees trivial. A 10Mb binary XML file can be loaded
from disk **and** queried in less than a few milliseconds.

The binary XML is not supposed to be small. It's usually about half the size of
the text XML data where a lot of the tag content is duplicated, but can actually
be larger than the original XML file. This isn't important; the fast query speed
and the ability to mmap strings without copies more than makes up for the larger
on-disk size. If you want to compress your XML, this library probably isn't for
you -- just use gzip -- it gives you an almost a perfect compression ratio for
data like this.

For example:

    $ xb-tool compile fedora.xmlb fedora.xml.gz

    $ du -h fedora.xml*
    12M         fedora.xmlb
    3.6M        fedora.xml.gz

    $ xb-tool query fedora.xmlb "components/component[@type=desktop]/id[text()=firefox.desktop]"
    RESULT: firefox.desktop
    real        0m0.011s
    user        0m0.010s
    sys         0m0.001s

XPath
=====

This library only implements a tiny subset of XPath. See the examples for the
full list, but it's basically restricted to element_name, attributes and text.

We will use the following XML document in the examples below.

    <?xml version="1.0" encoding="UTF-8"?>
    <bookstore>
      <book>
        <title lang="en">Harry Potter</title>
        <price>29.99</price>
      </book>
      <book percentage="99">
        <title lang="en">Learning XML</title>
        <price>39.95</price>
      </book>
    </bookstore>

Selecting Nodes
---------------

XPath uses path expressions to select nodes in an XML document. The only thing
that libxmlb can return are nodes.

| Example | Description | Supported |
| --- | --- | --- |
| `/bookstore` | Returns the root bookstore element | ✔ |
| `/bookstore/book` | Returns all `book` elements | ✔ |
| `//book` | Returns books no matter where they are | ✖ |
| `bookstore//book` | Returns books that are descendant of `bookstore` | ✖ |
| `@lang` | Returns attributes that are named `lang` | ✖ |
| `/bookstore/.` | Returns the `bookstore` node | ✖ |
| `/bookstore/book/*` | Returns all `title` and `price` nodes of each `book` node | ✔ |
| `/bookstore/book/child::*` | Returns all `title` and `price` nodes of each `book` node | ✔ |
| `/bookstore/book/title/..` | Returns the `book` nodes with a title | ✔ |
| `/bookstore/book/parent::*` | Returns `bookstore`, the parent of `book` | ✔ |
| `/bookstore/book/parent::bookstore` | Returns the parent `bookstore` of `book` | ✖ |

Predicates
----------

Predicates are used to find a specific node or a node that contains a specific
value. Predicates are always embedded in square brackets.

| Example | Description | Supported |
| --- | --- | --- |
| `/bookstore/book[1]` | Returns the first book element | ✔ |
| `/bookstore/book[first()]` | Returns the first book element | ✔ |
| `/bookstore/book[last()]` | Returns the last book element | ✔ |
| `/bookstore/book[last()-1]` | Returns the last but one book element | ✖ |
| `/bookstore/book[position()<3]` | Returns the first two books | ✔ |
| `/bookstore/book[upper-case(text())=='HARRY POTTER']` | Returns the first book | ✔ |
| `/bookstore/book[@percentage>=90]` | Returns the book with `>=` 90% completion | ✔ |
| `/bookstore/book/title[@lang]` | Returns titles with an attribute named `lang` | ✔ |
| `/bookstore/book/title[@lang='en']` | Returns titles that have a `lang`equal `en` | ✔ |
| `/bookstore/book/title[@lang!='en']` | Returns titles that have a `lang` not equal `en` | ✔ |
| `/bookstore/book/title[@lang<='zz_ZZ']` | Returns titles that `lang` <= `zz_ZZ` | ✔ |
| `/bookstore/book[price>35.00]` | Returns the books with a price greater than 35 | ✖ |
| `/bookstore/book[price>35.00]/title` | Returns the titles that have a price greater than 35 | ✖ |
| `/bookstore/book/title[text()='Learning XML']` | Returns the book node with matching content | ✔ |

Compilation
----------

libxmlb is a standard meson project.  It can be compiled using the following basic steps:

```
# meson build
# ninja -C build
# ninja -C build install
# ldconfig
```

This will by default install the library into `/usr/local`. On some Linux distributions you may
need to configure the linker path in `/etc/ld.so.conf` to be able to locate it.
The call to `ldconfig` is needed to refresh the linker cache.
