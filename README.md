
# baton |bəˈtän|

### noun, A short stick or staff or something resembling one.

Client programs and API for use with iRODS (Integrated Rule-Oriented
Data System).

baton is intended as a supplement to the command line client programs
(ils, imeta etc.) provided with a standard iRODS installation. Its
focus is metadata operations for iRODS collections and data objects.
baton is spelled with a lower case letter 'b'.

## The features it provides are:

- A single JSON format for listing results, composing queries and
  performing updates.

- Listing of data objects and collections as JSON, including their
  metadata (AVUs), file size, access control lists (ACLs) and creation
  and modification timestamps.

- Queries on metadata, on access control lists (ACLs), creation and
  modification timestamps and timestamp ranges. The full range of
  iRODS query operators is supported.

- Optional unbuffered IO for IPC via pipes, with fine-grained error
  reporting for batch operations.

- Simplified API over the iRODS general query API to ease construction
  of new custom queries.

- Downloading data objects as files or embedded in JSON (if data
  object content is UTF-8).

For further details see the man pages or manual, which is available
online at http://wtsi-npg.github.io/baton/

## iRODS compatibility:

Compatible with iRODS 4.1.x, 4.2.x and 4.3.x

  |baton version | Compatible iRODS versions|
  |--------------|--------------------------|
  | 2.0.x        | 4.1.x - 4.2.7            |
  | 2.1.x        | 4.1.x - 4.2.7            |
  | 3.0.x        | 4.2.7 - 4.2.8            |
  | 3.1.x        | 4.2.7 - 4.2.9            |
  | 3.2.x        | 4.2.7 - 4.2.11           |
  | 3.3.x        | 4.2.7 - 4.2.11           |
  | 4.2.x        | 4.2.7 - 4.3.3            |


Note that building against iRODS 4.3.0 requires 

    LDFLAGS="-Wl,-z,muldefs"

due to https://github.com/irods/irods/issues/6448 (which is fixed
in iRODS 4.3.1 and later).

## Installation:

1. Install iRODS and the baton dependencies (Jansson) as described in
   their documentation.

2. If you have cloned the baton git repository, run autoconf to
   generate the configure script. If you have downloaded a release tarball,
   the configure script is included, so proceed to step 3.

3. Run the configure script

4. Build

5. Install, including HTML manual and manpage.

The steps are summarised below:

    autoreconf -i  # Generate the configure script
    ./configure    # Configure
    make           # Build
    make install   # Install

If you have iRODS headers and libraries installed in a non-standard
place, you will need to set the CPPFLAGS and LDFLAGS environmment
variables appropriately. E.g.

    CPPFLAGS="-I/path/to/irods/headers" LDFLAGS="-L/path/to/irods/libraries" ./configure
    make CPPFLAGS="-I/path/to/irods/headers" LDFLAGS="-L/path/to/irods/libraries"

## Development:

To set up a development environment in Docker, use the provided
docker-compose.yml and corresponding Dockerfile. These create a Linux
C development and testing environment with an accompanying iRODS server.
The recommended way to use this is by remote development in the container
using VSCode. VSCode's manual explains how to do this, see:

    https://code.visualstudio.com/docs/remote/containers

To run the test suite:

    make check

If you have run configure with the optional --enable-coverage flag
you can generate test coverage statistics with lcov.

    make check-coverage


## Synopsis:

For full details of the JSON accepted and returned by the programs in
baton, see the manual in the doc directory.


## Dependencies:

- iRODS   https://github.com/irods/irods , versions 4.1.x, 4.2.x, 4.3.x
- Jansson https://github.com/akheron/jansson.git , versions >= 2.6

### Optional dependencies:

- Sphinx http://sphinx-doc.org/               (for the manual and manpages).
- check  http://check.sourceforge.net/        (for unit tests), versions >= 0.9.10
- lcov   http://ltp.sourceforge.net/coverage/ (for test coverage analysis)
- jq     http://stedolan.github.io/jq/        (for processing input and output)
