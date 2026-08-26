# CLOUDURP15J7 — true night atmosphere

The useful architectural idea from EnricoMonese/DayNightCycle is retained without porting Unity-specific APIs: solar elevation separately controls sky exposure and atmosphere thickness rather than using one colour blend for everything.

- astronomical environment remains the single day/night authority
- deep-night sky exposure is reduced before display gamma
- day/night atmosphere thickness transitions independently
- night horizon uses dark blue atmospheric haze instead of the old warm-grey floor
- weather does not become a second night darkening system
- Moon.png is rendered after sky tone mapping as a full, crisp moon
