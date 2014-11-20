
.. baton documentation master file, created by
   sphinx-quickstart on Thu Feb  6 09:45:48 2014.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to ``baton``'s documentation
====================================

baton \|bəˈtän|
	*noun, A short stick or staff or something resembling one.*

Client programs and API for use with `iRODS <http://www.irods.org>`_
(Integrated Rule-Oriented Data System).

``baton`` is intended as a supplement to the command line client
programs (``ils``, ``imeta`` etc.) provided with a standard iRODS
installation. Its provides the following features not included in
iRODS:

* A single JSON format for listing results, composing queries and
  performing updates.

* Listing of data objects and collections as JSON, including their
  metadata (AVUs), file size, access control lists (ACLs) and creation
  and modification timestamps.

* Queries on metadata, on access control lists (ACLs), creation and
  modification timestamps and timestamp ranges. The full range of
  iRODS query operators is supported.

* Fetching data object content inlined within JSON (text files only)
  or downloading data objects to local files.

* Unbuffered option for IPC via pipes with fine-grained error
  reporting for batch operations.

* Simplified API over the iRODS general query API to ease construction
  of new custom queries.


.. toctree::
   :maxdepth: 2

Obtaining and installing ``baton``
==================================

``baton`` may be downloaded as source code from its `GitHub homepage
<http://github.com/wtsi-npg/baton.git/>`_ . It is written using the
iRODS C API and requires a C compiler to build.

1. Install iRODS and the `Jansson JSON library
   <http://www.digip.org/jansson/>`_ ) as described in their
   documentation.

2. Generate the configure script. If you download a release tar.gz
   archive, you may skip this step because the configure script is
   included.

.. code-block:: sh

   autoreconf -i

3. Generate the makefiles (see INSTALL for arguments to configure).
   The path to iRODS must be the root directory of the iRODS installation,
   which defaults to ``/usr/local/lib/irods``.

.. code-block:: sh

   ./configure --with-irods=/path/to/irods

4. Compile

.. code-block:: sh

   make

5.  Optionally, run the test suite

.. code-block:: sh

   make check

6. If you have run configure with the optional --enable-coverage flag
   you can generate test coverage statistics with lcov.

.. code-block:: sh

   make check-coverage

7. Install, including HTML manual and manpage.

.. code-block:: sh

   make clean install



The ``baton`` suite of programs
===============================

``baton`` comprises several command-line programs, all of which accept
a stream of JSON objects on standard input (or from a file) and emit a
stream of JSON objects on standard output. The ``baton`` programs are:

* `baton-list`_

  Prints the paths, metadata and access control lists and timestamps
  of collections and data objects.

* `baton-metamod`_

  Add or remove specfic :term:`AVU` s in the metadata on collections
  and data objects.

* `baton-metasuper`

  Replace all :term:`AVU` s in the metadata on collections and data
  objects.

* `baton-metaquery`_

  Print the paths, metadata and access control lists of collections
  and data objects matching queries on metadata.

* `baton-chmod`_

  Modify the access control lists of collections and data objects.

All of the programs are designed to accept a stream of JSON objects,
one for each operation on a collection or data object. After each
operation is complete, the programs may be forced flush their output
(See the ``--unbuffered`` command line option.) to ease their use in
bidirectional communication via Unix pipes.

For details of how errors are handled, see :ref:`representing_errors`.


baton-list
----------

Synopsis:

.. code-block:: sh

   $ jq -n '{collection: "test"}' | baton-list

   $ jq -n '{collection: "/unit/home/user/", data_object: "a.txt"}' | baton-list

This program accepts JSON objects as described in
:ref:`representing_paths` and prints results in the same format. If
the target path is a data object, the printed result is also a data
object. However, if the target path is a collection, the printed
result is a JSON array of JSON objects containing one object
representing the target collection followed by one for each collection
or data object directly within the target.

Options
^^^^^^^

.. program:: baton-list
.. option:: --acl

  Print access control lists in the output, in the format described in
  :ref:`representing_path_permissions`.

.. program:: baton-list
.. option:: --avu

  Print AVU lists in output, in the format described in
  :ref:`representing_path_metadata`.


.. program:: baton-list
.. option:: --contents

  If the path is a collection print its contents, in the format described in
  :ref:`representing_path_metadata`. Only the paths contained directly
  within a collection are printed.


.. program:: baton-list
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-list
.. option:: --help

  Prints command line help.

.. program:: baton-list
.. option:: --size

  Print data object sizes in the output. These appear as JSON integers under
  the property 'size'.

.. program:: baton-list
.. option:: --timestamp

  Print data object timestamps in the output, in the format described in
  :ref:`representing_timestamps`.

.. program:: baton-list
.. option:: --unbuffered

  Flush output after each JSON object is processed.

.. program:: baton-list
.. option:: --verbose

  Print verbose messages to STDERR.

.. program:: baton-list
.. option:: --version

  Print the version number and exit.


baton-get
---------

Synopsis:

.. code-block:: sh

   $ jq -n '{collection: "/unit/home/user/", data_object: "a.txt"}' | baton-get

   $ jq -n '{collection: "/unit/home/user/", data_object: "a.txt", \
             directory: "/data/user/local" }'                      | baton-get --save

   $ jq -n '{collection: "/unit/home/user/", data_object: "a.txt", \
             directory: "/data/user/local",  file: "a_copy.txt"}'  | baton-get --save

This program accepts JSON objects as described in
:ref:`representing_paths` and prints results in the same format. The
target path must represent a data object. It's default mode is to JSON
encode the file content under the JSON property ``data`` in the output.

Some local path properties may be omitted from the input JSON and will
be inferred from the iRODS paths. If the ``file`` property is omitted,
the local file will take its name from the data object. If the
``directory`` property is omitted, the current working directory will
be used for any saved downloads.

``baton-get`` performs an on the-the-fly MD5 checksum of the data
object content as it processes. It compares this with the expected MD5
checksum held in iRODS and will raise an error if they do not
match. The program does not verify the checksum of the file(s) after
they have been written to disk.

Options
^^^^^^^

.. program:: baton-get
.. option:: --acl

  Print access control lists in the output, in the format described in
  :ref:`representing_path_permissions`.

.. program:: baton-get
.. option:: --avu

  Print AVU lists in output, in the format described in
  :ref:`representing_path_metadata`.

.. program:: baton-get
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-get
.. option:: --help

  Prints command line help.

.. program:: baton-get
.. option:: --raw

  Prints the contents of the data object instead of a JSON response. In this
  mode the program acts rather like the Unix program 'cat'. This mode, or the
  --save mode must be used for any file that is not UTF-8 encoded text.

.. program:: baton-get
.. option:: --save

  Saves the contents of the data object to a local file. The local file
  path is defined by the JSON input in the format described in
  :ref:`representing_local_paths`. Prints a JSON response to STDOUT for
  each file downloaded.

.. program:: baton-get
.. option:: --size

  Print data object sizes in the output. These appear as JSON integers under
  the property 'size'.

.. program:: baton-get
.. option:: --timestamp

  Print data object timestamps in the output, in the format described in
  :ref:`representing_timestamps`.

.. program:: baton-get
.. option:: --unbuffered

  Flush output after each JSON object is processed.

.. program:: baton-get
.. option:: --verbose

  Print verbose messages to STDERR.

.. program:: baton-get
.. option:: --version

  Print the version number and exit.


baton-metamod
-------------

Synopsis:

.. code-block:: sh

   $ jq -n '{collection: "test", data_object: "a.txt",     \
                  "avus": [{attribute: "x", value: "y"},   \
                           {attribute: "m", value: "n"}]}' | baton-metamod --operation add

   $ jq -n '{collection: "test", data_object: "a.txt",    \
                   avus: [{attribute: "x", value: "y"},   \
                          {attribute: "m", value: "n"}]}' | baton-metamod --operation rem

This program accepts JSON objects as described in
:ref:`representing_paths` and adds or removes matching metadata as
described in :ref:`representing_path_metadata`.

Options
^^^^^^^

.. program:: baton-metamod
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-metamod
.. option:: --help

  Prints command line help.

.. program:: baton-metamod
.. option:: --operation <operation name>

  The operation to perform; one of ``add`` or ``remove``.

.. program:: baton-metamod
.. option:: --unbuffered

  Flush output after each JSON object is processed.

.. program:: baton-metamod
.. option:: --verbose

  Print verbose messages to STDERR.

.. program:: baton-metamod
.. option:: --version

  Print the version number and exit.


baton-metaquery
---------------

Synopsis:

.. code-block:: sh

   $ jq -n '{avus: []}' | baton-metaquery

   $ jq -n '{collection: "test", avus: []}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "x", value: "y"}]}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "x", value: "y"},   \
                    {attribute: "m", value: "n"}]}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "v", value: "100", operator: "n<"},     \
                    {attribute: "w", value: "n%",  operator: "like"}]}' | baton-metaquery

This program accepts JSON objects as described in
:ref:`representing_query_metadata` and prints results in the format
described in :ref:`representing_paths` . Additionally, it accepts an
optional ``collection`` property which limits the scope of the
associated metadata query to returning only those results that lie
somewhere under that collection.


Options
^^^^^^^

.. program:: baton-metaquery
.. option:: --acl

  Print access control lists in the output, in the format described in
  :ref:`representing_path_permissions`.

.. program:: baton-metaquery
.. option:: --avu

  Print AVU lists in the output, in the format described in
  :ref:`representing_path_metadata`.

.. program:: baton-metaquery
.. option:: --coll

   Limit the search to collection metadata only.

.. program:: baton-metaquery
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-metaquery
.. option:: --help

  Prints command line help.

.. program:: baton-metaquery
.. option:: --obj

   Limit the search to data object metadata only.

.. program:: baton-metaquery
.. option:: --size

  Print data object sizes in the output. These appear as JSON integers under
  the property 'size'.

.. program:: baton-metaquery
.. option:: --timestamp

  Print timestamp lists in the output, in the format described in
  :ref:`representing_timestamps`.

.. program:: baton-metaquery
.. option:: --unbuffered

  Flush output after each JSON object is processed.

.. program:: baton-metaquery
.. option:: --verbose

  Print verbose messages to STDERR.

.. program:: baton-metaquery
.. option:: --version

  Print the version number and exit.

.. program:: baton-metaquery
.. option:: --zone <zone name>

   Query in a specific zone.


baton-chmod
-----------

.. code-block:: sh

   $ jq -n '{collection: "test",                            \
                access: [{owner: "public", level: "null"},  \
                         {owner: "admin",  level: "own"}]}' | baton-chmod --recurse

   $ jq -n '{collection: "test", data_object: "a.txt"          \
                 access: [{owner: "oscar",  level: "read"},    \
                          {owner: "victor", level: "write"}]}' | baton-chmod

Options
^^^^^^^

.. program:: baton-chmod
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-chmod
.. option:: --help

  Prints command line help.

.. program:: baton-chmod
.. option:: --recurse

  Recurse into collections. Defaults to false.

.. program:: baton-chmod
.. option:: --unbuffered

  Flush output after each JSON object is processed.

.. program:: baton-chmod
.. option:: --verbose

  Print verbose messages to STDERR.

.. program:: baton-chmod
.. option:: --version

  Print the version number and exit.


.. _representing_paths:

Representing data objects and collections
=========================================

Data objects and collections are represented as JSON objects. A
collection is identified by the ``collection`` property or its shorter
synonym ``c``. For example, the current iRODS working collection may
be represented by either of:

.. code-block:: json

   {"collection": "."}
   {"coll": "."}

The value associated with the ``collection`` property may be any iRODS
path, absolute or relative, in UTF-8 encoding. A trailing slash on
collections is permitted. This behaviour is different from iRODS
icommands which do not recognise collections specified with a trailing
slash:

.. code-block:: json

  {"collection": "/unit/home/user/λ/"}
  {"collection": "/unit/home/",
     "contents": [{"collection": "/unit/home/user/λ/"}]}

A collection's contents may be represented by the ``contents``
property. This is a JSON list which contains JSON objects representing
other collections and data objects. A ``contents`` property on a data
object JSON object has no meaning and will be ignored.

A data object is identified by having both a ``collection`` (or
``coll``) property and a ``data_object`` property or its shorter
synonym ``obj``. For example, a file in the current iRODS working
collection may be represented by either of:

.. code-block:: json

  {"collection:" ".", "data_object": "README.txt"}
  {"coll:" ".", "obj": "README.txt"}

The value associated with the ``data_object`` property may be any
iRODS data object name, in UTF-8 encoding. The full path of the data
object may be recreated by concatenating the ``collection`` and
``data_object`` values. Data objects reported by listing or searches
will may information on the file size under the ``size`` property. The
value is a JSON integer indicating the file size in bytes as given in
the ICAT database.

.. code-block:: json

  {"collection:" ".", "data_object": "README.txt", "size": 123456}

The above JSON representations of collections and data objects are
sufficient to be passed to baton's ``baton-list`` program, which
fulfils a similar role to the iRODS ``ils`` program, listing data
objects and collection contents.

``baton`` ignores JSON properties that it does not recognise, therefore it
is harmless to include extra properties in program input.

.. _representing_local_paths:

Representing Local Paths
========================

Copies of data objects on a filesystem local to the client are
represented as JSON objects. A directory is identified by the
``directory`` property or is shorter synonym ``dir``.

For example, the current working directory may be represented by
either of:

.. code-block:: json

   {"directory": "."}
   {"dir": "."}

The value associated with the ``directory`` property may be any path,
absolute or relative, in UTF-8 encoding.

A file is identified by having both a ``directory`` (or ``dir``)
property and a ``file`` property. For example, a file in the current
working directory may be represented by either of:

.. code-block:: json

  {"directory:" ".", "file": "README.txt"}
  {"dir:" ".", "file": "README.txt"}

The value associated with the ``file`` property may be any iRODS data
object name, in UTF-8 encoding. The full path of the local file may be
recreated by concatenating the ``directory`` and ``file`` values.

A JSON object may have both iRODS path and local path properties,
thereby indicating a mapping between a data object and a local
file. The ``data_object`` and ``file`` values are not required to be
identical.

.. code-block:: json

  {"collection": ".", "data_object": "README.txt",
   "directory:" ".",  "file": "README.txt"}


.. _representing_metadata:

Representing metadata
=====================

.. _representing_path_metadata:

Metadata on data objects and collections
----------------------------------------

Metadata attribute/value/units tuples are represented for both
collections and data objects as a JSON array of objects under the
``avus`` property. Each :term:`AVU` JSON object within the array must
have at least an ``attribute`` and a ``value`` property. A ``units``
property may be used where an :term:`AVU` has defined units. The
values associated with all of these properties are strings. These
properties have shorter synonyms ``a``, ``v`` and ``u``, respectively.

.. code-block:: json

   {"collection": "/unit/home/user",
          "avus": [{"attribute": "a", "value": "b", "units": "c"},
                   {"attribute": "m", "value": "n"},
                   {"attribute": "x", "value": "y", "units": "z"}]}

   {"coll": "/unit/home/user",
    "avus": [{"a": "a", "v": "b", "u": "c"},
             {"a": "m", "v": "n"},
             {"a": "x", "v": "y", "u": "z"}]}


Any path that has no metadata may have an ``avus`` property with an
empty JSON array as its value.

.. _representing_query_metadata:

Metadata queries
----------------

This format for :term:`AVU` s is used both in reporting existing
metadata and in specifying baton queries. In the latter case a JSON
:term:`AVU` object acts as a selector in the query, with multiple
:term:`AVU` s being combined with logical ``AND`` and the comparison
operator defaulting to ``=``. As with the iRODS ``imeta`` program,
units are ignored in queries.

For example, the following JSON may be passed to the baton
``baton-metaquery`` program:

.. code-block:: json

   {"avus": [{"attribute": "a", "value": "b", "units": "c"},
             {"attribute": "x", "value": "y"}]}

This is equivalent to a query using the iRODS ``imeta`` program of::

  a = b and x = y

In cases where the ``=`` operator is not appropriate, an additional
``operator`` property may be supplied to override the default. The
available operators are:

* ``=``
* ``like``
* ``not like``
* ``in``
* ``>``
* ``n>`` (numeric coercion)
* ``<``
* ``n<`` (numeric coercion)
* ``>=``
* ``n>=`` (numeric coercion)
* ``<=``
* ``n<=`` (numeric coercion)

The ``in`` operator requires that the attribute's value in the query
be a JSON list of possible strings to test. A successful match will be
any metadata having one of those strings as a value for that
attribute. e.g.

.. code-block:: json

   {"avus": [{"attribute": "a", "value": ["a", "b", "c"], "operator": "in"}]}

To modify the above query to return results only where the attribute
``x`` has values numerically less than 100, the JSON :term:`AVU` would
become:

.. code-block:: json

   {"avus": [{"attribute": "a", "value": "b", "units": "c"},
             {"attribute": "x", "value": "100", "operator": "n<"}]}

This is equivalent to a query using the iRODS ``imeta`` program of::

  a = b and x n< y

The shorter synonym ``o`` may be used as an alternative operator
property so that the query above becomes:

.. code-block:: json

   {"avus": [{"a": "a", "v": "b", "u": "c"},
             {"a": "x", "v": "100", "o": "n<"}]}


A query specified may include a ``collection`` property. The effect of
this is to both direct the query to a specific zone and possibly to
limit results to items contained in a specific collection. This
property allows queries to different zones to be mixed in the input
stream to a single invocation of the query program.

To direct the query to a specific zone, only the zone portion of the
path is required:

.. code-block:: json

  {"avus": [{"a": "a", "v": "b"}],
   "coll": "/public"}

This will direct the query to the ``public`` zone. If the query
program has been started with a ``--zone`` argument explicitly, that
will take precedence over the zone provided in the query.

To limit results to a specific collection:

.. code-block:: json

  {"avus": [{"a": "a", "v": "b"}],
   "coll": "/public/seq/a/b/c"}

This will limit query results to those with AVU ``a = b`` located in
the collection '/public/seq/a/b/c'. Beware that this type of query may
have significantly poor performance in the ICAT generated SQL.


.. _representing_timestamps:

Representing timestamps
=======================

The ICAT database records timestamps indicating when collections and
data objects were created and last modified. These are represented for
both collections and data objects as a JSON array of objects under the
``timestamp`` property. Each timestamp within the array must have at
least a ``created`` or a ``modified`` property. The values associated
with these properties are `ISO8601 datetime strings
<https://en.wikipedia.org/wiki/ISO_8601>`_. These properties have
shorter synonyms ``c``, ``m``, respectively.

.. code-block:: json

   {"collection": "/unit/home/user",
    "timestamps": [{"created":  "2014-01-01T00:00:00"},
                   {"modified": "2014-01-01T00:00:00"}]}

Data objects may have replicates in iRODS. Where data object
timestamps are reported by ``baton``, only the values for the
lowest-numbered replicate are reported. The replicate number is given
in the timestamp object as a JSON integer.

.. code-block:: json

   {"collection": "/unit/home/user",
    "data_object": "foo.txt",
    "timestamps": [{"created":  "2014-01-01T00:00:00", "replicate": 0},
                   {"modified": "2014-01-01T00:00:00", "replicate": 0}]}


.. _representing_query_timestamps:

Timestamp queries
-----------------

The format described in :ref:`representing_timestamps` is used in both
reporting existing timestamps and in refining baton metadata queries.
In the latter case a timestamp acts as a selector in the query, with
multiple timestamps being combined with logical ``AND`` and the
comparison operator defaulting to ``=``. Time ranges may be queried by
using multiple timestamp objects having the same key (``created`` or
``modified``).

In most cases the the ``=`` operator is not appropriate and additional
``operator`` property should be supplied to override the default. The
available operators are described in
:ref:`representing_query_metadata`. The numeric coercing operators are
preferred because the ICAT database stores timestamps as seconds since
the epoch.

For example, to limit query results to anything created on or after
2014-01-01, the syntax would be:

.. code-block:: json

   {"timestamps": [{"created": "2014-01-01T00:00:00", "operator": "n>="}]}

To limit query results to anything created before 2014-01-01 and
modified in 2014-03, the syntax would be:

.. code-block:: json

   {"timestamps": [{"created":  "2014-01-01T00:00:00", "operator": "n<"},
                   {"modified": "2014-03-01T00:00:00", "operator": "n>="},
                   {"modified": "2014-03-31T00:00:00", "operator": "n<"}]}

.. _representing_permissions:

Representing Access Control Lists
=================================

.. _representing_path_permissions:

ACLs of data objects and collections
------------------------------------

:term:`ACL` s are represented for both collections and data objects as
a JSON array of objects under the ``access`` property. Each JSON
:term:`ACL` object within the array must have at least an ``owner``
and a ``level`` property.

.. code-block:: json

   {"collection": "/unit/home/user",
        "access": [{"owner": "user",   "level": "own"},
                   {"owner": "public", "level": "read"}]}

The value associated with the ``owner`` property must be an iRODS user
or group name. The value associated with the ``level`` property must
be an iRODS symbolic access level string (i.e. "null", "read", "write"
or "own").


.. _representing_query_permissions:

ACL queries
-----------

The format described in :ref:`representing_permissions` is used in
both reporting existing permissions and in refining baton metadata
queries. In the latter case an ACL acts as a selector in the query.

To limit query results to anything owned by ``user1``, the syntax
would be:

.. code-block:: json

   {"access": [{"owner": "user1", "level": "own"}]}

However, care is required; there are two important points to bear in
mind when composing ACL queries:

1. The ICAT database queries will match exact permissions only and
   will not return results for items whose permissions are subsumed
   under the query permission. For example, a query for items for
   which user ``user1`` has permissions ``read`` will not return any
   items where ``user1`` has permissions ``read-write`` or ``own``,
   despite the fact that the latter subsume ``read``.

2. Only one permission clause per user may be included in the
   query. This is because ICAT combines queries using ``AND`` which,
   combined with its limitations described above, would yield queries
   that always return an empty set. E.g. the following query will
   never return any results:

.. code-block:: json

   {"access": [{"owner": "user1", "level": "own"},
               {"owner": "user1", "level": "read"}]}


.. _representing_errors:

Representing Errors
===================

When an error occurs while processing a particular JSON object in a
stream, instead of printing the usual result object, the input JSON
object is printed to standard output with an ``error`` property added
to it. The value associated with this property is a JSON object that
includes the iRODS error code under a ``code`` property and (where
possible) and an error message under a ``message`` property.

.. code-block:: json

   {"collection": "test",
         "error": {"code": -310000,
                   "message": "Path '/unit/home/user/test' does not exist (or lacks access permission)"}}

In this way, errors remain associated with the inputs that caused
them.


The ``baton`` Cookbook
======================

``baton`` benefits from the `lightweight command-line JSON processor
'jq' <http://stedolan.github.io/jq/>`_ . In the following examples,
``jq`` is used to ease the *de novo* creation of JSON in the shell and
to process the results returned by ``baton``.

.. topic:: List the contents of the current working collection

   List the contents of the current working collection as flat paths
   suitable for passing to standard Unix filters.

.. code-block:: sh

   jq -n '{coll: "."}' | baton-list | jq -r '.[] | .collection + "/" + .data_object'

Result:

.. code-block:: sh

    /unit/home/user/data/
    /unit/home/user/data/f1.txt
    /unit/home/user/data/f2.txt
    /unit/home/user/data/f3.txt
    /unit/home/user/data/a/
    /unit/home/user/data/b/
    /unit/home/user/data/c/


.. topic:: List the contents of a collection by metadata (I)

   List all the collections and data objects located directly within
   the collection ``data``, that have the attribute ``attr_a``
   somewhere in their AVUs:

.. code-block:: sh

   jq -n '{coll: "data"}' | baton-list --avu | jq 'map(select(.avus[] | select(.attribute == "attr_a")))'

Result:

.. code-block:: json

    [
      {
        "avus": [
          {
            "value": "value_b",
            "attribute": "attr_b"
          },
          {
            "value": "value_a",
            "attribute": "attr_a"
          },
          {
            "value": "value_c",
            "attribute": "attr_c"
          }
        ],
        "data_object": "f1.txt",
        "collection": "/unit/home/user/data",
        "size": 0
      }
    ]

.. topic:: List the contents of a collection by metadata (II)

   A similar operation to the previous one, except using an iRODS
   query with scope limited to the collection ``data`` or its
   subcollections, recursively.

.. code-block:: sh

   jq -n '{coll: "data", avus: [{a: "attr_a", v: "%", o: "like"}]}' | baton-metaquery --avu | jq '.'


.. code-block:: json

    [
      {
        "avus": [
          {
            "value": "value_b",
            "attribute": "attr_b"
          },
          {
            "value": "value_a",
            "attribute": "attr_a"
          },
          {
            "value": "value_c",
            "attribute": "attr_c"
          }
        ],
        "data_object": "f1.txt",
        "collection": "/unit/home/user/data",
        "size": 0
      }
    ]


.. topic:: List the metadata on data objects

   List the metadata on all data objects in the collection ``data``:

.. code-block:: sh

    jq -n '{coll: "data"}' | baton-list --avu | jq 'map(select(.data_object))[]'

.. code-block:: json

    {
      "avus": [
        {
          "value": "value_b",
          "attribute": "attr_b"
        },
        {
          "value": "value_a",
          "attribute": "attr_a"
        },
        {
          "value": "value_c",
          "attribute": "attr_c"
        }
      ],
      "data_object": "f1.txt",
      "collection": "/unit/home/user/data",
      "size": 0
    }
    {
      "avus": [
        {
          "value": "value_c",
          "attribute": "attr_c"
        }
      ],
      "data_object": "f2.txt",
      "collection": "/unit/home/user/data",
      "size": 0
    }
    {
      "avus": [
        {
          "value": "value_c",
          "attribute": "attr_c"
        }
      ],
      "data_object": "f3.txt",
      "collection": "/unit/home/user/data",
      "size": 0
    }


.. topic:: Run a simple iRODS metadata query

   Run a simple iRODS metadata query for collections and data objects
   having both 'attr_a' = 'value_a' and 'attr_c' == 'value_c'. The
   final pass through ``jq`` simply pretty-prints the result. The
   result contains no AVU information because the ``--avu`` argument
   to ``baton-metaquery`` was not used:

.. code-block:: sh

   jq -n '{avus: [{a: "attr_a", v: "value_a"},   \
                  {a: "attr_c", v: "value_c"}]}' | baton-metaquery | jq '.'

Result:

.. code-block:: json

    [
      {
        "data_object": "f1.txt",
        "collection": "/unit/home/user/data",
        "size": 0
      }
    ]


.. topic:: Run a simple iRODS metadata query with timestamp selection

   Run an iRODS metadata query for collections and data objects having
   both 'attr_a' = 'value_a' and 'attr_c' == 'value_c' that were
   created before 2014-03-01T00:00:00 and modified between
   2014-03-17T00:00:00 and 2014-03-18T00:00:00:

.. code-block:: sh

   jq -n '{avus: [{a: "attr_a", v: "value_a"},           \
                  {a: "attr_c", v: "value_c"}],          \
           time: [{c: "2014-03-01T00:00:00", o: "n<"},   \
                  {m: "2014-03-17T00:00:00", o: "n>="},  \
                  {m: "2014-03-18T00:00:00", o: "n<"}]}' | baton-metaquery | jq '.'

Result:

.. code-block:: json

    [
      {
        "data_object": "f1.txt",
        "collection": "/unit/home/user/data",
        "size": 0
      }
    ]

..
   .. doxygenfile:: baton.h
      :project: baton


.. glossary::

   ACL
       **A**\ccess **C**\ontrol **L**\ist.

   AVU
       **A**\ttribute **V**\alue **U**\nit metadata tuple.


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
