# CLOUDURP15G — gradual rain + brighter noon clouds

The previous regional weather G channel was normalized against the current authored rainfall peak. Consequently any non-zero rain amount converted G into an almost amount-independent cell mask, creating an immediate cloud-darkening step. CLOUDURP15G changes G to physical current rain / 80 mm/h.

Cloud optical rain response is now smooth, thickness extinction is softer, cloud ambient illumination is stronger, and high solar altitude increases direct cloud illumination. The radar/weather field remains the authority for where rain-bearing cloud cells exist.
