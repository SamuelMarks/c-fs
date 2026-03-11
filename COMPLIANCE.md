# `std::filesystem` Compliance Table

This document tracks the compliance and feature parity of the `c-fs` library against the C++17 `std::filesystem` standard library. 

| `std::filesystem` Feature | `c-fs` Equivalent | Status | Notes |
|---------------------------|-------------------|--------|-------|
| **Classes & Types**       |                   |        |       |
| [`path`](https://en.cppreference.com/w/cpp/filesystem/path) | `cfs_path`        | ✅   | Comprehensive path parsing & manipulation |
| [`filesystem_error`](https://en.cppreference.com/w/cpp/filesystem/filesystem_error) | `cfs_error_code`  | ✅   | Core POSIX `errc` mapping |
| [`directory_entry`](https://en.cppreference.com/w/cpp/filesystem/directory_entry) | `cfs_directory_entry` | ✅ | |
| [`directory_iterator`](https://en.cppreference.com/w/cpp/filesystem/directory_iterator) | `cfs_directory_iterator` | ✅ | |
| [`recursive_directory_iterator`](https://en.cppreference.com/w/cpp/filesystem/recursive_directory_iterator) | `cfs_recursive_directory_iterator` | ✅ | |
| [`file_status`](https://en.cppreference.com/w/cpp/filesystem/file_status) | `cfs_file_status` | ✅   | |
| [`space_info`](https://en.cppreference.com/w/cpp/filesystem/space_info) | `cfs_space_info`  | ✅   | |
| [`file_type`](https://en.cppreference.com/w/cpp/filesystem/file_type) | `cfs_file_type`   | ✅   | |
| [`perms`](https://en.cppreference.com/w/cpp/filesystem/perms) | `cfs_perms`       | ✅   | |
| [`perm_options`](https://en.cppreference.com/w/cpp/filesystem/perm_options) | `cfs_perm_options` | ✅   | |
| [`copy_options`](https://en.cppreference.com/w/cpp/filesystem/copy_options) | `cfs_copy_options`| ✅   | |
| [`directory_options`](https://en.cppreference.com/w/cpp/filesystem/directory_options) | `cfs_directory_options` | ✅   | |
| [`file_time_type`](https://en.cppreference.com/w/cpp/filesystem/file_time_type) | `cfs_file_time_type` | ✅| |
| **Functions**             |                   |        |       |
| [`absolute`](https://en.cppreference.com/w/cpp/filesystem/absolute) | `cfs_absolute`    | ✅   | |
| [`canonical`](https://en.cppreference.com/w/cpp/filesystem/canonical) | `cfs_canonical`   | ✅   | |
| [`copy`](https://en.cppreference.com/w/cpp/filesystem/copy) | `cfs_copy`        | ✅   | |
| [`copy_file`](https://en.cppreference.com/w/cpp/filesystem/copy_file) | `cfs_copy_file`   | ✅   | |
| [`copy_symlink`](https://en.cppreference.com/w/cpp/filesystem/copy_symlink) | `cfs_copy_symlink` | ✅   | |
| [`create_directory`](https://en.cppreference.com/w/cpp/filesystem/create_directory) | `cfs_create_directory` | ✅ | |
| [`create_directories`](https://en.cppreference.com/w/cpp/filesystem/create_directory) | `cfs_create_directories` | ✅ | |
| [`create_hard_link`](https://en.cppreference.com/w/cpp/filesystem/create_hard_link) | `cfs_create_hard_link` | ✅ | |
| [`create_symlink`](https://en.cppreference.com/w/cpp/filesystem/create_symlink) | `cfs_create_symlink` | ✅ | |
| [`create_directory_symlink`](https://en.cppreference.com/w/cpp/filesystem/create_directory_symlink) | `cfs_create_directory_symlink` | ✅ | |
| [`current_path`](https://en.cppreference.com/w/cpp/filesystem/current_path) | `cfs_current_path` | ✅  | Set/Get both available |
| [`equivalent`](https://en.cppreference.com/w/cpp/filesystem/equivalent) | `cfs_equivalent`  | ✅   | |
| [`exists`](https://en.cppreference.com/w/cpp/filesystem/exists) | `cfs_exists_path` | ✅   | |
| [`file_size`](https://en.cppreference.com/w/cpp/filesystem/file_size) | `cfs_file_size`   | ✅   | Includes async definition |
| [`hard_link_count`](https://en.cppreference.com/w/cpp/filesystem/hard_link_count) | `cfs_hard_link_count` | ✅   | |
| [`is_block_file`](https://en.cppreference.com/w/cpp/filesystem/is_block_file) | `cfs_is_block_file` | ✅ | |
| [`is_character_file`](https://en.cppreference.com/w/cpp/filesystem/is_character_file) | `cfs_is_character_file` | ✅ | |
| [`is_directory`](https://en.cppreference.com/w/cpp/filesystem/is_directory) | `cfs_is_directory` | ✅  | |
| [`is_empty`](https://en.cppreference.com/w/cpp/filesystem/is_empty) | `cfs_is_empty_path` | ✅ | |
| [`is_fifo`](https://en.cppreference.com/w/cpp/filesystem/is_fifo) | `cfs_is_fifo`     | ✅   | |
| [`is_other`](https://en.cppreference.com/w/cpp/filesystem/is_other) | `cfs_is_other`    | ✅   | |
| [`is_regular_file`](https://en.cppreference.com/w/cpp/filesystem/is_regular_file) | `cfs_is_regular_file` | ✅ | |
| [`is_socket`](https://en.cppreference.com/w/cpp/filesystem/is_socket) | `cfs_is_socket`   | ✅   | |
| [`is_symlink`](https://en.cppreference.com/w/cpp/filesystem/is_symlink) | `cfs_is_symlink`  | ✅   | |
| [`last_write_time`](https://en.cppreference.com/w/cpp/filesystem/last_write_time) | `cfs_last_write_time` | ✅ | |
| [`permissions`](https://en.cppreference.com/w/cpp/filesystem/permissions) | `cfs_permissions` | ✅   | |
| [`proximate`](https://en.cppreference.com/w/cpp/filesystem/proximate) | `cfs_proximate`   | ✅   | FS-aware missing, Lexical wrapper added |
| [`read_symlink`](https://en.cppreference.com/w/cpp/filesystem/read_symlink) | `cfs_read_symlink` | ✅   | |
| [`relative`](https://en.cppreference.com/w/cpp/filesystem/relative) | `cfs_relative`    | ✅   | FS-aware missing, Lexical wrapper added |
| [`remove`](https://en.cppreference.com/w/cpp/filesystem/remove) | `cfs_remove`      | ✅   | Includes async definition |
| [`remove_all`](https://en.cppreference.com/w/cpp/filesystem/remove) | `cfs_remove_all`  | ✅   | |
| [`rename`](https://en.cppreference.com/w/cpp/filesystem/rename) | `cfs_rename`      | ✅   | |
| [`resize_file`](https://en.cppreference.com/w/cpp/filesystem/resize_file) | `cfs_resize_file` | ✅   | |
| [`space`](https://en.cppreference.com/w/cpp/filesystem/space) | `cfs_space`       | ✅   | |
| [`status`](https://en.cppreference.com/w/cpp/filesystem/status) | `cfs_status`      | ✅   | |
| [`status_known`](https://en.cppreference.com/w/cpp/filesystem/status_known) | `cfs_status_known` | ✅   | |
| [`symlink_status`](https://en.cppreference.com/w/cpp/filesystem/status) | `cfs_symlink_status` | ✅| |
| [`temp_directory_path`](https://en.cppreference.com/w/cpp/filesystem/temp_directory_path) | `cfs_temp_directory_path` | ✅ | |
| [`weakly_canonical`](https://en.cppreference.com/w/cpp/filesystem/weakly_canonical) | `cfs_weakly_canonical` | ✅   | |
| [`hash_value`](https://en.cppreference.com/w/cpp/filesystem/path/hash_value) |                   | ➖ | C++ specific STL feature |