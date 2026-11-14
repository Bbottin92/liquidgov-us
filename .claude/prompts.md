# Reusable Prompts

## New Feature Module
Create a new modular feature under liquidgov-platform/src/site/features/<name> or a new lib under libs/. Include typed headers, implementation, CMake integration, a dedicated test file, and update sources.cmake if needed. Follow naming.toml.

## Review Deploy Script
Review this deploy script for correctness against naming.toml restart rules, security (no secrets), and consistency with existing city-remote.sh style scripts. Propose minimal changes.

## Add Test
Add a new C++ unit test file following the pattern in tests/*_test.cpp. Link the appropriate libraries. Keep existing tests frozen.

## Update Persistence
Append a dated bullet to the relevant heading in Persistence/notes.md summarizing the decision or change just made.
