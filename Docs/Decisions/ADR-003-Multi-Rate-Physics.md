# ADR-003: Multi-Rate Physics

**Status:** Accepted

The complete rigid-body world does not run at 1000 Hz. The general world uses a lower fixed rate while tires, suspension, drivetrain details, and force feedback may run independent high-rate substeps.

This preserves fidelity where it matters without multiplying the cost of every world object and collision by four or more.
