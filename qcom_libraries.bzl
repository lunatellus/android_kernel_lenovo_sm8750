load(":kleaf-scripts/libraries_register.bzl", "create_library_registry")
load(":libraries.bzl", "register_libraries")

library_registry = create_library_registry()
register_libraries(library_registry)
