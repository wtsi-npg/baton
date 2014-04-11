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

* Queries on metadata, paths and access control lists combined.

* A simple JSON format to describe data objects, collections, metadata
  queries and query results.

* Efficient re-use of connections to the iRODS server for batch
  operations, while maintaining fine-grained error detection.

* Output from baton programs can be input to other baton programs to
  create Unix pipelines.

* Simplified API over the iRODS general query API to ease construction
  of new custom queries.

Contents:

.. toctree::
   :maxdepth: 2

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

  Print access control lists in output, in the format described in
  :ref:`representing_path_permissions`.

.. program:: baton-list
.. option:: --avu

  Print AVU lists in output, in the format described in
  :ref:`representing_path_metadata`.

.. program:: baton-list
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-list
.. option:: --help

  Prints command line help.

.. program:: baton-list
.. option:: --unbuffered

  Flush output after each JSON object is processed.


baton-metamod
-------------

Synopsis:

.. code-block:: sh

   $ jq -n '{collection: "test", data_object: "a.txt",      \
                  "avus": [{attribute: "x", value: "y"},   \
                           {attribute: "m", value: "n"}]}' | baton-metamod --operation add

   $ jq -n '{collection: "test", data_object: "a.txt",      \
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


baton-metaquery
---------------

Synopsis:

.. code-block:: sh

   $ jq -n '{avus: []}' | baton-metaquery

   $ jq -n '{collection: "test", avus: []}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "x", value: "y"}]}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "x", value: "y"},   \
                    {attribute: "m", value: "n"}]}' | baton-metaquery

   $ jq -n '{avus: [{attribute: "v", value: "100", operator: "n<"},    \
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

  Print access control lists in output, in the format described in
  :ref:`representing_path_permissions`.

.. program:: baton-metaquery
.. option:: --avu

  Print AVU lists in output, in the format described in
  :ref:`representing_path_metadata`.

.. program:: baton-metaquery
.. option:: --file <file name>

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

.. program:: baton-metaquery
.. option:: --help

  Prints command line help.

.. program:: baton-metaquery
.. option:: --timestamp

  Print timestamp lists in output, in the format described in
  :ref:`representing_timestamps`.

.. program:: baton-metaquery
.. option:: --unbuffered

  Flush output after each JSON object is processed.

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
will contain information on the file size under the ``size``
property. The value is a JSON integer indicating the file size in
bytes as given in the ICAT database.

.. code-block:: json

  {"collection:" ".", "data_object": "README.txt", "size": 123456}

The above JSON representations of collections and data objects are
sufficient to be passed to baton's ``baton-list`` program, which
fulfils a similar role to the iRODS ``ils`` program, listing data
objects and collection contents.

``baton`` ignores JSON properties that it does not recognise, therefore it
is harmless to include extra properties in program input.

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
* ``>``
* ``n>`` (numeric coercion)
* ``<``
* ``n<`` (numeric coercion)
* ``>=``
* ``n>=`` (numeric coercion)
* ``<=``
* ``n<=`` (numeric coercion)

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

.. _representing_timestamps:

Representing timestamps
=======================

The ICAT database records timestamps indicating when collections and
data objects were created and last modified. These are represented for
both collections and data objects as a JSON array of objects under the
``timestamp`` property. Each timestamp within the array must have at
least a ``created`` or a ``modified`` property. The values associated
with these properties are ISO8601 datetime strings. These
properties have shorter synonyms ``c``, ``m``, respectively.

.. code-block:: json

   {"coll": "/unit/home/user",
    "timestamps": [{"created":  "2014-01-01T00:00:00"},
                   {"modified": "2014-01-01T00:00:00"}]}

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


.. topic:: List the contents of a collection by metadata I

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

.. topic:: List the contents of a collection by metadata II

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

   Run a simple iRODS metadata query for collections and data objects
   having both 'attr_a' = 'value_a' and 'attr_c' == 'value_c' that were
   created before 2014-03-01T00:00:00 and modified between
   2014-03-17T00:00:00 and 2014-03-18T00:00:00:



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
