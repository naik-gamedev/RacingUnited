# ADR-001: One Heritage Engine Executable Loads Modules

**Status:** Accepted

Heritage Engine is one executable that loads Racing United or another module. Modules own content and gameplay; they do not require separate copies of the engine executable.

This avoids divergent engines, duplicated settings systems, and incompatible fixes while allowing modules to select different gameplay and physics providers.
