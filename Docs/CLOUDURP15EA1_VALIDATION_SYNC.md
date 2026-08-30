# CLOUDURP15EA1 validation sync

CLOUDURP15EA strengthened the existing selective stochastic cloud denoiser. The older CLOUDURP15E8 architecture guard still required the exact E8 classifier threshold literals, contradicting its intended role as an architecture guard and causing a false validation failure.

EA1 changes only that validator: it now requires the mild/strong stochastic classifier stages to exist without pinning their historical E8 numeric thresholds. The dedicated CLOUDURP15EA guard continues to pin the current 11x11 spatial denoiser and 0.9995 / 0.9998 / 0.99995 temporal values.

No cloud shader/runtime behavior is modified by EA1.
