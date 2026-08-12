# ADR-041 — Spatial tread temperature and wear state

**Status:** Accepted for TIRE08  
**Date:** 2026-08-09

## Decision

Heritage tire thermal/wear history is represented by a bounded 16 circumferential sector x
3 lateral band field (inside/center/outside), for 48 state cells per tire. These cells are
history/state only; they do not multiply the number of Magic Formula force evaluations.

TIRE07 remains authoritative for mean tread/carcass/gas energy and inflation pressure.
TIRE08 stores local tread-surface temperature deviation relative to the shared bulk tread and
remaining tread depth. Slip energy is deposited into the current circumferential contact sector
and pressure/camber-weighted lateral bands. Local diffusion/relaxation redistributes surface
heat. Wear consumes local dissipated energy with bounded load/temperature sensitivity.

A locked/sliding wheel therefore heats and wears a localized sector, producing flat-spot
history; a rotating wheel distributes the work around the circumference. Severe local wear may
reduce the effective single-MF contact friction scale, while physical flat-spot radius/vibration
is deferred to a later explicit contact-geometry mechanism.

## Rationale

48 small scalar state cells are inexpensive even for large race grids, while preserving the
spatial history required for shoulder temperatures, pressure/camber wear patterns and true
localized lock-up wear. This captures the useful concept of spatial tire-pad simulation without
copying another simulator's proprietary implementation and without turning every tire into 48
full contact/MF solvers.

## Data provenance

Racing United's `[HERITAGE_TREAD_STATE]` parameters are explicit synthetic development data.
They are neither measured historical Pirelli data nor proprietary Live for Speed/Simcenter
coefficients. Better identified data can replace the values without changing the provider
architecture.
