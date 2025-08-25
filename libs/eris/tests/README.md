# Eris Tests

The `Metaserver_unittest` test suite uses an embedded mock metaserver. The
mock listens on a randomly assigned UDP port and emulates enough of the
metaserver protocol for the library to initialise. Tests inject the mock's
address into `Eris::Meta`, so no external services are required when running
the tests locally or in CI.
