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

baton is intended as a supplement to the command line client programs
(``ils``, ``imeta`` etc.) provided with a standard iRODS
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

* `json-list`_

  Prints the paths, metadata and access control lists of collections
  and data objects.

* `json-metalist`_

  Print the metadata on collections and data objects.

* `json-metamod`_

  Add or remove specfic :term:`AVU` s in the metadata on collections
  and data objects.

* `json-metasuper`

  Replace all :term:`AVU` s in the metadata on collections and data
  objects.

* `json-metaquery`_

  Print the paths, metadata and access control lists of collections
  and data objects matching queries on metadata.

* `json-chmod`_

  Modify the access control lists of collections and data objects.

All of the programs are designed to accept a stream of JSON objects,
one for each operation on a collection or data object. After each
operation is complete, the programs may be forced flush their output
(See the ``--unbuffered`` command line option.) to ease their use in
bidirectional communication via Unix pipes.

For details of how errors are handled, see :ref:`representing_errors`.

json-list
---------

Synopsis:

.. code-block:: sh

   $ echo '{"collection": "test"}' | json-list

   $ echo '{"collection": "/unit/home/user/", \
            "data_object": "a.txt"}' | json-list

This program accepts JSON objects as described in
:ref:`representing_paths` and prints results in the same format. If
the target path is a data object, the printed result is also a data
object. However, if the target path is a collection, the printed
result is a JSON array of JSON objects containing one object
representing the target collection followed by one for each collection
or data object directly within the target.

Options
^^^^^^^

* ``--acl``

  Print access control lists in output, in the format described in
  :ref:`representing_path_permissions`.

* ``--avu``

  Print AVU lists in output, in the format described in
  :ref:`representing_path_metadata`.

* ``--file <file name>``

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

* ``--help``

  Prints command line help.

* ``--unbuffered``

  Flush output after each JSON object is processed.

json-metalist
-------------

Synopsis:

.. code-block:: sh

   $ echo '{"collection": "."}' | json-metalist

   $ echo '{"collection": "test"}' | json-metalist

   $ echo '{"collection": "test", \
            "data_object": "a.txt"}' | json-metalist

   $ echo '{"collection": "test/c/"}' | json-metalist

This program accepts JSON objects as described in
:ref:`representing_paths` and prints results in the same format, with
metadata as described in :ref:`representing_path_metadata`.

Options
^^^^^^^

* ``--file <file name>``

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

* ``--help``

  Prints command line help.

* ``--unbuffered``

  Flush output after each JSON object is processed.

json-metamod
------------

Synopsis:

.. code-block:: sh

   $ echo '{"collection": "test",  \
            "data_object": "a.txt" \
            "avus": [{"attribute": "x", "value": "y"},   \
                     {"attribute": "m", "value": "n"}]}' \
                | json-metamod --operation add

   $ echo '{"collection": "test",  \
            "data_object": "a.txt" \
            "avus": [{"attribute": "x", "value": "y"},   \
                     {"attribute": "m", "value": "n"}]}' \
                | json-metamod --operation rem


This program accepts JSON objects as described in
:ref:`representing_paths` and adds or removes matching metadata as
described in :ref:`representing_path_metadata`.

Options
^^^^^^^

* ``--file <file name>``

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

* ``--help``

  Prints command line help.

* ``--operation <operation name>``

  The operation to perform; one of ``add`` or ``remove``.

* ``--unbuffered``

  Flush output after each JSON object is processed.

json-metaquery
--------------

Synopsis:

.. code-block:: sh

   $ echo '{"avus": []}' | json-metaquery

   $ echo '{"collection": "test", "avus": []}' | json-metaquery

   $ echo '{"avus": [{"attribute": "x", "value": "y"}]}' | json-metaquery

   $ echo '{"avus": [{"attribute": "x", "value": "y"}, \
                     {"attribute": "m", "value": "n"}]}' | json-metaquery

   $ echo '{"avus": [{"attribute": "v", "value": "100", \
                      "operator": "n<"},                \
                     {"attribute": "w", "value": "n%",  \
                      "operator": "like"}]}' | json-metaquery

This program accepts JSON objects as described in
:ref:`representing_query_metadata` and prints results in the format
described in :ref:`representing_paths` . Additionally, it accepts an
optional ``collection`` property which limits the scope of the
associated metadata query to returning only those results that lie
somewhere under that collection.

json-chmod
----------

.. code-block:: sh

   $ echo '{"collection": "test",
            "access": [{"owner": "public", "level": "null"},  \
                       {"owner": "admin",  "level": "own"}]}' \
                | json-chmod --recurse

   $ echo '{"collection": "test",  \
            "data_object": "a.txt" \
            "access": [{"owner": "oscar",  "level": "read"},    \
                       {"owner": "victor", "level": "write"}]}' \
                | json-chmod

Options
^^^^^^^

* ``--file <file name>``

  The JSON file describing the data objects and collections. Optional,
  defaults to STDIN.

* ``--help``

  Prints command line help.

* ``--recurse``

  Recurse into collections. Defaults to false.

* ``--unbuffered``

  Flush output after each JSON object is processed.

.. _representing_paths:

Representing data objects and collections
=========================================

Data objects and collections are represented as JSON objects. A
collection is identified by the ``collection`` property. For example,
the current iRODS working collection may be represented:

.. code-block:: json

   {"collection": "."}

The value associated with the ``collection`` property may be any iRODS
path, absolute or relative, in UTF-8 encoding. A trailing slash on
collections is permitted. This behaviour is different from iRODS
icommands which do not recognise collections specified with a trailing
slash:

.. code-block:: json

  {"collection": "/unit/home/user/λ/"}

A data object is identified both a ``collection`` and ``data_object``
property. For example, a file in the current iRODS working collection
may be represented:

.. code-block:: json

  {"collection:" ".", "data_object": "README.txt"}

The value associated with the ``data_object`` property may be any
iRODS data object name, in UTF-8 encoding. The full path of the data
object may be recreated by concatenating the ``collection`` and
``data_object`` values.

The above JSON representations of collections and data objects are
sufficient to be passed to baton's ``json-list`` program, which
fulfils a similar role to the iRODS ``ils`` program, listing data
objects and collection contents.

baton ignores JSON properties that it does not recognise, therefore it
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
values associated with all of these properties are strings.

.. code-block:: json

   {"collection": "/unit/home/user",
    "avus": [{"attribute": "a", "value": "b", "units": "c"},
             {"attribute": "m", "value": "n"},
             {"attribute": "x", "value": "y", "units": "z"}]}

Any path that has no metadata may have an ``avus`` property with an
empty JSON array as its value.

.. _representing_query_metadata:

Metadata queries
-------------------

This format for :term:`AVU` s is used both in reporting existing
metadata and in specifying baton queries. In the latter case a JSON
:term:`AVU` object acts as a selector in the query, with multiple
:term:`AVU` s being combined with logical ``AND`` and the comparison
operator defaulting to ``=``. As with the iRODS ``imeta`` program,
units are ignored in queries.

For example, the following JSON may be passed to the baton
``json-metaquery`` program:

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
              "message": "Path '/unit/home/user/test' does not \
   exist (or lacks access permission)"}}

In this way, errors remain associated with the inputs that caused
them.

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
