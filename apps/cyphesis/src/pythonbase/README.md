# Python Base Utilities

## Logging Context

`PythonLogGuard` injects contextual prefixes into spdlog output using thread-local storage. The guard stores a callback that returns a string prefix. When active, every log message on the same thread is automatically prefixed with that string.

```cpp
PythonLogGuard guard([](){ return "[script] "; });
spdlog::info("hello"); // produces: [script] hello
```

The formatter is installed automatically by `init_python_api`, but custom loggers can opt-in by calling `python_log::install_python_log_formatter(logger)`. Because the prefix is thread-local, concurrent threads may log independently without corrupting each other's output.
