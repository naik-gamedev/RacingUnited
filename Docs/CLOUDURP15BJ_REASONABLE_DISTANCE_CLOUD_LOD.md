# CLOUDURP15BJ – Reasonable Distance Cloud LOD

Builds on CLOUDURP15BI by improving the quality of distant but still visually important clouds. Instead of dropping too early into the cheapest far-cloud representation, the raymarch now uses an intermediate mid-distance quality band with lower mip bias, softer degradation and more samples. Near/overhead clouds keep the BI shading improvements, while truly far horizon clouds still dissolve into haze.
