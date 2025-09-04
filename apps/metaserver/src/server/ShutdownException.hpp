#ifndef METASERVER_SHUTDOWNEXCEPTION_HPP
#define METASERVER_SHUTDOWNEXCEPTION_HPP

#include <stdexcept>
#include <string>

/**
 * Exception used to signal a graceful shutdown of the metaserver.
 */
class ShutdownException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

#endif // METASERVER_SHUTDOWNEXCEPTION_HPP
