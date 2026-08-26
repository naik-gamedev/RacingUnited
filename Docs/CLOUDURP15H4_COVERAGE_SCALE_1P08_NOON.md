# CLOUDURP15H4 — coverage scale + 1.08x noon boost

The authored cloud-cover slider is recalibrated in `PrecipitationField::regionalWeatherSample`, so the shared weather authority—not merely the cloud shader—uses the new scale.

- 0% -> clear endpoint
- 2% -> old 50% cloud formation authority
- 50% -> old ~74.5% authority
- 100% -> full authority

Noon cloud direct-light boost is 1.08x. H3 ambient/extinction/density tuning remains unchanged.
