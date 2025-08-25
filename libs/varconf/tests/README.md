# Varconf Tests

These tests write temporary configuration files to directories created with `std::filesystem::temp_directory_path` and remove them after execution. This prevents test artifacts from remaining in the working tree.
