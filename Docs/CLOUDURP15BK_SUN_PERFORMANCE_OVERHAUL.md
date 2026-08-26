# CLOUDURP15BK — Sun Performance Overhaul

This pass removes the non-functional expensive sun-shaft path and substantially reduces directional-light and cloud-sun costs. PCSS/PCF budgets are adaptive by cascade, shadow storage/resolution is lighter, back-facing fragments skip all direct-sun shadow work, and cloud sunlight/cloud-shadow sampling is reduced. The existing horizon and cloud appearance work remains authoritative.
