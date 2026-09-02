# SUSP06A — OPT01 Retirement Convergence Hotfix

SUSP06 turned the former planned double-wishbone source path into a real compiled
implementation. The legacy OPT01 ZIP-overlay convergence list still deleted that
path before validation, so a normal build removed the implementation it was then
asked to validate.

SUSP06A removes that one now-live source path from the retirement list. It does
not relax SUSP06 validation, change the double-wishbone solver, or resurrect any
other retired scaffold.
